/*
 * llm_parser.c - LLM-based Input Parser for NAGI
 *
 * Implementation using llama.cpp for natural language understanding.
 *
 * NOTE: This file requires llama.cpp to be linked. If building without
 * LLM support, define NAGI_NO_LLM to use stub implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "llm_parser.h"
#include "llm_context.h"

#ifndef NAGI_NO_LLM
/* llama.cpp headers */
#include "llama.h"
#include "common.h"
#endif

/* Maximum vocabulary entries */
#define MAX_VOCAB_ENTRIES 2048

/* Internal state structure */
struct llm_state {
#ifndef NAGI_NO_LLM
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;
#endif
    int initialized;
    char last_error[256];

    /* System prompt */
    char system_prompt[LLM_MAX_PROMPT_SIZE];

    /* Game-specific info */
    char game_info[1024];

    /* Vocabulary */
    llm_vocab_entry_t vocab[MAX_VOCAB_ENTRIES];
    int vocab_count;
};

/* Global instances */
llm_state_t *g_llm_state = NULL;
llm_config_t g_llm_config = {
    .model_path = "",
    .context_size = LLM_DEFAULT_CONTEXT_SIZE,
    .batch_size = LLM_DEFAULT_BATCH_SIZE,
    .n_threads = LLM_DEFAULT_THREADS,
    .temperature = 0.7f,
    .top_p = 0.9f,
    .top_k = 40,
    .max_tokens = 128,
    .use_gpu = 0,
    .verbose = 0
};

/* Default system prompt for AGI games */
static const char *DEFAULT_SYSTEM_PROMPT =
    "You are a parser for a classic Sierra AGI adventure game. "
    "Your task is to convert natural language player input into "
    "AGI-compatible verb-noun commands.\n\n"
    "Rules:\n"
    "1. Extract the main VERB and NOUN from the input\n"
    "2. Use simple words that match the game's vocabulary\n"
    "3. If unsure, pick the most likely interpretation\n"
    "4. Output format: VERB,NOUN (e.g., 'look,door' or 'get,key')\n"
    "5. For movement, use: north, south, east, west, up, down\n"
    "6. Common verbs: look, get, open, close, use, talk, give, push, pull\n\n";

/*
 * Set error message
 */
static void set_error(const char *fmt, ...)
{
    if (g_llm_state) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_llm_state->last_error, sizeof(g_llm_state->last_error), fmt, args);
        va_end(args);
    }
}

#ifndef NAGI_NO_LLM
/*
 * Initialize the LLM parser with llama.cpp
 */
int llm_parser_init(const char *model_path, const llm_config_t *config)
{
    struct llama_model_params model_params;
    struct llama_context_params ctx_params;

    if (g_llm_state && g_llm_state->initialized) {
        fprintf(stderr, "LLM Parser: Already initialized\n");
        return 1;
    }

    /* Allocate state */
    g_llm_state = (llm_state_t *)calloc(1, sizeof(llm_state_t));
    if (!g_llm_state) {
        fprintf(stderr, "LLM Parser: Failed to allocate state\n");
        return 0;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&g_llm_config, config, sizeof(llm_config_t));
    }
    if (model_path) {
        strncpy(g_llm_config.model_path, model_path, LLM_MAX_MODEL_PATH - 1);
    }

    /* Set default system prompt */
    strncpy(g_llm_state->system_prompt, DEFAULT_SYSTEM_PROMPT,
            sizeof(g_llm_state->system_prompt) - 1);

    /* Initialize llama backend */
    llama_backend_init();

    /* Load model */
    model_params = llama_model_default_params();
    model_params.n_gpu_layers = g_llm_config.use_gpu ? 99 : 0;

    printf("LLM Parser: Loading model from %s...\n", g_llm_config.model_path);
    g_llm_state->model = llama_load_model_from_file(g_llm_config.model_path, model_params);
    if (!g_llm_state->model) {
        set_error("Failed to load model: %s", g_llm_config.model_path);
        fprintf(stderr, "LLM Parser: %s\n", g_llm_state->last_error);
        free(g_llm_state);
        g_llm_state = NULL;
        return 0;
    }

    /* Create context */
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = g_llm_config.context_size;
    ctx_params.n_batch = g_llm_config.batch_size;
    ctx_params.n_threads = g_llm_config.n_threads;
    ctx_params.n_threads_batch = g_llm_config.n_threads;

    g_llm_state->ctx = llama_new_context_with_model(g_llm_state->model, ctx_params);
    if (!g_llm_state->ctx) {
        set_error("Failed to create context");
        fprintf(stderr, "LLM Parser: %s\n", g_llm_state->last_error);
        llama_free_model(g_llm_state->model);
        free(g_llm_state);
        g_llm_state = NULL;
        return 0;
    }

    /* Create sampler */
    g_llm_state->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_top_k(g_llm_config.top_k));
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_top_p(g_llm_config.top_p, 1));
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_temp(g_llm_config.temperature));
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_dist(42));

    g_llm_state->initialized = 1;
    printf("LLM Parser: Initialized successfully\n");
    printf("  Context size: %d\n", g_llm_config.context_size);
    printf("  Batch size: %d\n", g_llm_config.batch_size);
    printf("  Threads: %d\n", g_llm_config.n_threads);

    return 1;
}

/*
 * Shutdown the LLM parser
 */
void llm_parser_shutdown(void)
{
    if (!g_llm_state) return;

    if (g_llm_state->sampler) {
        llama_sampler_free(g_llm_state->sampler);
    }
    if (g_llm_state->ctx) {
        llama_free(g_llm_state->ctx);
    }
    if (g_llm_state->model) {
        llama_free_model(g_llm_state->model);
    }

    llama_backend_free();

    free(g_llm_state);
    g_llm_state = NULL;

    printf("LLM Parser: Shutdown complete\n");
}

/*
 * Check if LLM is ready
 */
int llm_parser_ready(void)
{
    return g_llm_state && g_llm_state->initialized;
}

/*
 * Parse player input using the LLM
 */
int llm_parser_parse(const char *input, const char *context, llm_parse_result_t *result)
{
    char prompt[LLM_MAX_PROMPT_SIZE];
    char *response;
    int n_tokens;
    llama_token *tokens;
    int n_prompt_tokens;

    if (!g_llm_state || !g_llm_state->initialized) {
        set_error("LLM not initialized");
        return 0;
    }

    memset(result, 0, sizeof(llm_parse_result_t));

    /* Build prompt */
    snprintf(prompt, sizeof(prompt),
        "%s\n"
        "%s\n"
        "Game context:\n%s\n\n"
        "Player input: \"%s\"\n\n"
        "Parse this input into VERB,NOUN format:\n",
        g_llm_state->system_prompt,
        g_llm_state->game_info,
        context ? context : "(no context)",
        input);

    /* Tokenize prompt */
    n_tokens = llama_n_ctx(g_llm_state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));

    n_prompt_tokens = llama_tokenize(g_llm_state->model, prompt, strlen(prompt),
                                      tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        set_error("Failed to tokenize prompt");
        free(tokens);
        return 0;
    }

    /* Clear KV cache */
    llama_kv_cache_clear(g_llm_state->ctx);

    /* Create batch and process prompt */
    struct llama_batch batch = llama_batch_get_one(tokens, n_prompt_tokens);

    if (llama_decode(g_llm_state->ctx, batch) != 0) {
        set_error("Failed to decode prompt");
        free(tokens);
        return 0;
    }

    /* Generate response */
    response = result->llm_response;
    int response_len = 0;
    int max_response = LLM_MAX_RESPONSE_SIZE - 1;

    for (int i = 0; i < g_llm_config.max_tokens && response_len < max_response; i++) {
        llama_token new_token = llama_sampler_sample(g_llm_state->sampler, g_llm_state->ctx, -1);

        /* Check for end of generation */
        if (llama_token_is_eog(g_llm_state->model, new_token)) {
            break;
        }

        /* Convert token to text */
        char piece[64];
        int piece_len = llama_token_to_piece(g_llm_state->model, new_token,
                                              piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < max_response) {
            memcpy(response + response_len, piece, piece_len);
            response_len += piece_len;
        }

        /* Decode next token */
        batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(g_llm_state->ctx, batch) != 0) {
            break;
        }
    }
    response[response_len] = '\0';

    free(tokens);

    /* Parse response to extract verb/noun */
    result->success = llm_parser_response_to_words(response,
        (int[]){result->verb_id, result->noun_id}, 2);

    if (result->success) {
        result->confidence = 0.8f;  /* TODO: Calculate actual confidence */
        snprintf(result->canonical, sizeof(result->canonical), "%s %s",
                 result->verb_str, result->noun_str);
    }

    return result->success;
}

/*
 * Generate a response using the LLM
 */
int llm_parser_generate(const char *prompt, const char *context,
                        char *output, int output_size)
{
    char full_prompt[LLM_MAX_PROMPT_SIZE];
    int n_tokens;
    llama_token *tokens;
    int n_prompt_tokens;
    int output_len = 0;

    if (!g_llm_state || !g_llm_state->initialized) {
        return 0;
    }

    /* Build full prompt */
    snprintf(full_prompt, sizeof(full_prompt),
        "%s\n\nContext:\n%s\n\n%s",
        g_llm_state->system_prompt,
        context ? context : "",
        prompt);

    /* Tokenize */
    n_tokens = llama_n_ctx(g_llm_state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));

    n_prompt_tokens = llama_tokenize(g_llm_state->model, full_prompt, strlen(full_prompt),
                                      tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return 0;
    }

    /* Clear KV cache and decode prompt */
    llama_kv_cache_clear(g_llm_state->ctx);
    struct llama_batch batch = llama_batch_get_one(tokens, n_prompt_tokens);

    if (llama_decode(g_llm_state->ctx, batch) != 0) {
        free(tokens);
        return 0;
    }

    /* Generate */
    for (int i = 0; i < g_llm_config.max_tokens && output_len < output_size - 1; i++) {
        llama_token new_token = llama_sampler_sample(g_llm_state->sampler, g_llm_state->ctx, -1);

        if (llama_token_is_eog(g_llm_state->model, new_token)) {
            break;
        }

        char piece[64];
        int piece_len = llama_token_to_piece(g_llm_state->model, new_token,
                                              piece, sizeof(piece), 0, true);
        if (piece_len > 0 && output_len + piece_len < output_size - 1) {
            memcpy(output + output_len, piece, piece_len);
            output_len += piece_len;
        }

        batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(g_llm_state->ctx, batch) != 0) {
            break;
        }
    }
    output[output_len] = '\0';

    free(tokens);
    return output_len;
}

#else /* NAGI_NO_LLM - Stub implementations */

int llm_parser_init(const char *model_path, const llm_config_t *config)
{
    (void)model_path;
    (void)config;

    g_llm_state = (llm_state_t *)calloc(1, sizeof(llm_state_t));
    if (!g_llm_state) return 0;

    strncpy(g_llm_state->system_prompt, DEFAULT_SYSTEM_PROMPT,
            sizeof(g_llm_state->system_prompt) - 1);
    g_llm_state->initialized = 1;

    printf("LLM Parser: Initialized (stub mode - no LLM support)\n");
    return 1;
}

void llm_parser_shutdown(void)
{
    if (g_llm_state) {
        free(g_llm_state);
        g_llm_state = NULL;
    }
}

int llm_parser_ready(void)
{
    return g_llm_state && g_llm_state->initialized;
}

int llm_parser_parse(const char *input, const char *context, llm_parse_result_t *result)
{
    (void)context;

    memset(result, 0, sizeof(llm_parse_result_t));

    /* Simple fallback parsing - just split on space */
    char *space = strchr(input, ' ');
    if (space) {
        int verb_len = space - input;
        if (verb_len > 63) verb_len = 63;
        strncpy(result->verb_str, input, verb_len);
        result->verb_str[verb_len] = '\0';

        strncpy(result->noun_str, space + 1, 63);
        result->noun_str[63] = '\0';

        result->success = 1;
    } else {
        strncpy(result->verb_str, input, 63);
        result->verb_str[63] = '\0';
        result->success = 1;
    }

    snprintf(result->canonical, sizeof(result->canonical), "%s %s",
             result->verb_str, result->noun_str);

    return result->success;
}

int llm_parser_generate(const char *prompt, const char *context,
                        char *output, int output_size)
{
    (void)prompt;
    (void)context;
    strncpy(output, "[LLM not available]", output_size - 1);
    output[output_size - 1] = '\0';
    return strlen(output);
}

#endif /* NAGI_NO_LLM */

/* ========== Common functions (both LLM and stub) ========== */

/*
 * Load vocabulary from AGI words.tok data
 */
int llm_parser_load_vocab(const u8 *words_data, int data_size)
{
    if (!g_llm_state) return 0;

    /* TODO: Parse words.tok format */
    /* The format is complex - for now use load_vocab_file instead */
    (void)words_data;
    (void)data_size;

    return 0;
}

/*
 * Load vocabulary from text file
 */
int llm_parser_load_vocab_file(const char *filename)
{
    FILE *f;
    char line[512];
    int count = 0;

    if (!g_llm_state) return 0;

    f = fopen(filename, "r");
    if (!f) {
        set_error("Could not open vocab file: %s", filename);
        return 0;
    }

    while (fgets(line, sizeof(line), f) && g_llm_state->vocab_count < MAX_VOCAB_ENTRIES) {
        char *id_str, *word_str, *syn_str, *type_str;
        char *saveptr;

        line[strcspn(line, "\r\n")] = '\0';

        /* Parse: word_id|word|synonyms|is_verb */
        id_str = strtok_r(line, "|", &saveptr);
        word_str = strtok_r(NULL, "|", &saveptr);
        syn_str = strtok_r(NULL, "|", &saveptr);
        type_str = strtok_r(NULL, "|", &saveptr);

        if (id_str && word_str) {
            llm_vocab_entry_t *entry = &g_llm_state->vocab[g_llm_state->vocab_count];
            entry->word_id = atoi(id_str);
            strncpy(entry->word, word_str, sizeof(entry->word) - 1);
            entry->word[sizeof(entry->word) - 1] = '\0';

            if (syn_str) {
                strncpy(entry->synonyms, syn_str, sizeof(entry->synonyms) - 1);
                entry->synonyms[sizeof(entry->synonyms) - 1] = '\0';
            } else {
                entry->synonyms[0] = '\0';
            }

            entry->is_verb = type_str ? atoi(type_str) : 0;

            g_llm_state->vocab_count++;
            count++;
        }
    }

    fclose(f);
    printf("LLM Parser: Loaded %d vocabulary entries\n", count);
    return count;
}

/*
 * Add vocabulary entry
 */
void llm_parser_add_vocab(int word_id, const char *word, const char *synonyms, int is_verb)
{
    if (!g_llm_state || g_llm_state->vocab_count >= MAX_VOCAB_ENTRIES) return;

    llm_vocab_entry_t *entry = &g_llm_state->vocab[g_llm_state->vocab_count];
    entry->word_id = word_id;
    strncpy(entry->word, word, sizeof(entry->word) - 1);
    entry->word[sizeof(entry->word) - 1] = '\0';

    if (synonyms) {
        strncpy(entry->synonyms, synonyms, sizeof(entry->synonyms) - 1);
        entry->synonyms[sizeof(entry->synonyms) - 1] = '\0';
    } else {
        entry->synonyms[0] = '\0';
    }

    entry->is_verb = is_verb;
    g_llm_state->vocab_count++;
}

/*
 * Find word by string
 */
int llm_parser_find_word(const char *word, int *is_verb)
{
    if (!g_llm_state) return -1;

    for (int i = 0; i < g_llm_state->vocab_count; i++) {
        llm_vocab_entry_t *entry = &g_llm_state->vocab[i];

        /* Check main word */
        if (strcasecmp(entry->word, word) == 0) {
            if (is_verb) *is_verb = entry->is_verb;
            return entry->word_id;
        }

        /* Check synonyms */
        if (entry->synonyms[0]) {
            char syn_copy[256];
            strncpy(syn_copy, entry->synonyms, sizeof(syn_copy) - 1);
            syn_copy[sizeof(syn_copy) - 1] = '\0';

            char *token = strtok(syn_copy, ",");
            while (token) {
                while (*token == ' ') token++;  /* Skip leading spaces */
                if (strcasecmp(token, word) == 0) {
                    if (is_verb) *is_verb = entry->is_verb;
                    return entry->word_id;
                }
                token = strtok(NULL, ",");
            }
        }
    }

    return -1;
}

/*
 * Set system prompt
 */
void llm_parser_set_system_prompt(const char *prompt)
{
    if (g_llm_state && prompt) {
        strncpy(g_llm_state->system_prompt, prompt,
                sizeof(g_llm_state->system_prompt) - 1);
        g_llm_state->system_prompt[sizeof(g_llm_state->system_prompt) - 1] = '\0';
    }
}

/*
 * Set game info
 */
void llm_parser_set_game_info(const char *game_info)
{
    if (g_llm_state && game_info) {
        strncpy(g_llm_state->game_info, game_info,
                sizeof(g_llm_state->game_info) - 1);
        g_llm_state->game_info[sizeof(g_llm_state->game_info) - 1] = '\0';
    }
}

/*
 * Set parameter
 */
void llm_parser_set_param(const char *param, const char *value)
{
    if (!param || !value) return;

    if (strcmp(param, "temperature") == 0) {
        g_llm_config.temperature = (float)atof(value);
    } else if (strcmp(param, "top_p") == 0) {
        g_llm_config.top_p = (float)atof(value);
    } else if (strcmp(param, "top_k") == 0) {
        g_llm_config.top_k = atoi(value);
    } else if (strcmp(param, "max_tokens") == 0) {
        g_llm_config.max_tokens = atoi(value);
    }
}

/*
 * Get last error
 */
const char *llm_parser_get_error(void)
{
    return g_llm_state ? g_llm_state->last_error : "Not initialized";
}

/*
 * Print vocabulary (debug)
 */
void llm_parser_print_vocab(void)
{
    if (!g_llm_state) return;

    printf("=== LLM Vocabulary (%d entries) ===\n", g_llm_state->vocab_count);
    for (int i = 0; i < g_llm_state->vocab_count && i < 50; i++) {
        llm_vocab_entry_t *entry = &g_llm_state->vocab[i];
        printf("%3d: [%d] %s (%s) %s\n",
               i, entry->word_id, entry->word,
               entry->is_verb ? "verb" : "noun",
               entry->synonyms);
    }
    if (g_llm_state->vocab_count > 50) {
        printf("... and %d more\n", g_llm_state->vocab_count - 50);
    }
}

/*
 * Print stats (debug)
 */
void llm_parser_print_stats(void)
{
    if (!g_llm_state) {
        printf("LLM Parser: Not initialized\n");
        return;
    }

    printf("=== LLM Parser Stats ===\n");
    printf("Initialized: %s\n", g_llm_state->initialized ? "yes" : "no");
    printf("Vocabulary: %d entries\n", g_llm_state->vocab_count);
    printf("Context size: %d\n", g_llm_config.context_size);
    printf("Temperature: %.2f\n", g_llm_config.temperature);
}

/*
 * Convert LLM response to word IDs
 */
int llm_parser_response_to_words(const char *response, int *word_ids, int max_words)
{
    char response_copy[LLM_MAX_RESPONSE_SIZE];
    char *token;
    int count = 0;

    if (!response || !word_ids || max_words <= 0) return 0;

    strncpy(response_copy, response, sizeof(response_copy) - 1);
    response_copy[sizeof(response_copy) - 1] = '\0';

    /* Remove any leading/trailing whitespace and newlines */
    char *start = response_copy;
    while (*start && (isspace(*start) || *start == '\n')) start++;

    char *end = start + strlen(start) - 1;
    while (end > start && (isspace(*end) || *end == '\n')) {
        *end = '\0';
        end--;
    }

    /* Parse comma-separated words */
    token = strtok(start, ",");
    while (token && count < max_words) {
        /* Trim whitespace */
        while (*token && isspace(*token)) token++;
        char *token_end = token + strlen(token) - 1;
        while (token_end > token && isspace(*token_end)) {
            *token_end = '\0';
            token_end--;
        }

        /* Look up word */
        int is_verb;
        int word_id = llm_parser_find_word(token, &is_verb);
        if (word_id >= 0) {
            word_ids[count++] = word_id;
        }

        token = strtok(NULL, ",");
    }

    return count;
}
