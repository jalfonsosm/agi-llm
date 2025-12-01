/*
 * llm_parser.c - LLM-based Input Parser for NAGI
 *
 * Implementation using llama.cpp for natural language understanding.
 *
 * NOTE: This file requires llama.cpp to be linked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "llm_parser.h"
#include "llm_context.h"

/* Maximum vocabulary entries (used by common code) */
#define MAX_VOCAB_ENTRIES 2048

static const char *DEFAULT_SYSTEM_PROMPT =
    "You are a command matcher for a classic Sierra AGI adventure game.\n"
    "Your task is to determine if a natural language player input matches a valid game command.\n\n"
    "Rules:\n"
    "1. Answer ONLY with 'yes' or 'no'.\n"
    "2. SUPPORT DIFFERENT LANGUAGES FOR INPUT: Translate commands to their English equivalents before matching.\n"
    "   Examples:\n"
    "   - 'mira el castillo' -> matches 'look,castle' -> yes\n"
    "   - 'coge la llave' -> matches 'get,key' -> yes\n"
    "   - 'abrir puerta' -> matches 'open,door' -> yes\n"
    "   - 'salta la cuerda' -> not in vocabulary -> no\n\n";

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
    .use_gpu = 1,
    .verbose = 0
};

#include "llama.h"

static void set_error(const char *fmt, ...);

struct llm_state {
    /* For real LLM backend */
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;

    /* Common fields */
    int initialized;
    char last_error[256];
    char system_prompt[LLM_MAX_PROMPT_SIZE];
    char game_info[1024];
    llm_vocab_entry_t vocab[MAX_VOCAB_ENTRIES];
    int vocab_count;

    /* KV Cache Management */
    llama_token *tokens_prev;
    int n_tokens_prev;
};

int llm_parser_init(const char *model_path, const llm_config_t *config)
{
    struct llama_model_params model_params;
    struct llama_context_params ctx_params;

    if (g_llm_state && g_llm_state->initialized) {
        fprintf(stderr, "LLM Parser: Already initialized\n");
        return 1;
    }

    /* Allocate state */
    g_llm_state = (llm_state_t *)calloc(1, sizeof(struct llm_state));
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
    g_llm_state->model = llama_model_load_from_file(g_llm_config.model_path, model_params);
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

    g_llm_state->ctx = llama_init_from_model(g_llm_state->model, ctx_params);
    if (!g_llm_state->ctx) {
        set_error("Failed to create context");
        fprintf(stderr, "LLM Parser: %s\n", g_llm_state->last_error);
        llama_model_free(g_llm_state->model);
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

    /* Allocate KV cache tracking buffer */
    g_llm_state->tokens_prev = (llama_token *)malloc(g_llm_config.context_size * sizeof(llama_token));
    g_llm_state->n_tokens_prev = 0;

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
        llama_model_free(g_llm_state->model);
    }

    llama_backend_free();

    free(g_llm_state->tokens_prev);
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
 * Generate a response using the LLM
 */
/*
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

    snprintf(full_prompt, sizeof(full_prompt),
        "%s\n\nContext:\n%s\n\n%s",
        g_llm_state->system_prompt,
        context ? context : "",
        prompt);

    n_tokens = llama_n_ctx(g_llm_state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));

    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(g_llm_state->model), full_prompt, (int)strlen(full_prompt),
                                      tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return 0;
    }

    {
        struct llama_batch batch = llama_batch_init(g_llm_config.batch_size, 0, 1);
        for (int i = 0; i < n_prompt_tokens; i += g_llm_config.batch_size) {
            int n_eval = n_prompt_tokens - i;
            if (n_eval > g_llm_config.batch_size) {
                n_eval = g_llm_config.batch_size;
            }

            batch.n_tokens = n_eval;
            for (int k = 0; k < n_eval; k++) {
                batch.token[k] = tokens[i + k];
                batch.pos[k] = i + k;
                batch.n_seq_id[k] = 1;
                batch.seq_id[k][0] = 0;
                batch.logits[k] = false;
            }
            
            if (i + n_eval == n_prompt_tokens) {
                batch.logits[n_eval - 1] = true;
            }

            if (llama_decode(g_llm_state->ctx, batch) != 0) {
                llama_batch_free(batch);
                free(tokens);
                return 0;
            }
        }
        llama_batch_free(batch);
    }

    int response_len = 0;
    int max_response = LLM_MAX_RESPONSE_SIZE;
    char *response = (char *)malloc(max_response);
    
    struct llama_batch batch_gen = llama_batch_init(1, 0, 1);

    printf("Debug: Starting generation loop...\n");
    while (response_len < max_response - 1) {
        llama_token new_token = llama_sampler_sample(g_llm_state->sampler, g_llm_state->ctx, -1);
        llama_sampler_accept(g_llm_state->sampler, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(g_llm_state->model), new_token)) {
            printf("Debug: EOG token encountered.\n");
            break;
        }

        char piece[64];
        int piece_len = llama_token_to_piece(llama_model_get_vocab(g_llm_state->model), new_token,
                                              piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < max_response) {
            memcpy(response + response_len, piece, piece_len);
            response_len += piece_len;
            piece[piece_len] = '\0';
            printf("Debug: Generated piece: '%s'\n", piece);
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + response_len; // Approximate pos
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = 0;
        batch_gen.logits[0] = true;

        if (llama_decode(g_llm_state->ctx, batch_gen) != 0) {
            printf("Debug: llama_decode failed in generation loop.\n");
            break;
        }
    }
    printf("Debug: Generation complete. Response: '%s'\n", response);
    llama_batch_free(batch_gen);
    output[output_len] = '\0';

    free(tokens);
    return output_len;
}
*/

/* Common error setter used by both real and stub implementations */
static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (g_llm_state) {
        vsnprintf(g_llm_state->last_error, sizeof(g_llm_state->last_error), fmt, args);
    } else {
        /* Fallback: print to stderr */
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        fprintf(stderr, "LLM Parser Error: %s\n", buf);
    }
    va_end(args);
}

/*
 * Set game info
 */
/*
void llm_parser_set_game_info(const char *game_info)
{
    if (g_llm_state && game_info) {
        strncpy(g_llm_state->game_info, game_info,
                sizeof(g_llm_state->game_info) - 1);
        g_llm_state->game_info[sizeof(g_llm_state->game_info) - 1] = '\0';
    }
}
*/

/*
 * Set parameter
 */
/*
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
*/

/*
 * Helper: Get word string from word ID
 */
static const char *get_word_string(int word_id)
{
    if (!g_llm_state) return NULL;
    
    for (int i = 0; i < g_llm_state->vocab_count; i++) {
        if (g_llm_state->vocab[i].word_id == word_id) {
            return g_llm_state->vocab[i].word;
        }
    }
    return NULL;
}

/*
 * Helper: Check whether an input matches an expected AGI word list
 * Uses semantic matching: asks LLM "does input match command?"
 * Returns 1 if match, 0 otherwise
 */
int llm_parser_matches_expected(const char *input, const char *context,
                                const int *expected_word_ids, int expected_count,
                                float min_confidence)
{
    char prompt[LLM_MAX_PROMPT_SIZE];
    char expected_command[256];
    char response_buf[256];
    int n_tokens, n_prompt_tokens;
    llama_token *tokens;

    (void)min_confidence; /* Unused for now */
    (void)context;        /* Unused for now */

    if (!llm_parser_ready()) return 0;
    if (expected_count == 0) return 0;

    /* Build expected command string from word IDs */
    expected_command[0] = '\0';
    for (int i = 0; i < expected_count; i++) {
        int word_id = expected_word_ids[i];

        /* Skip wildcards (word ID 1 = ANY) - always match */
        /*
        if (word_id == 1) 
            return 1; // TODO: clean the user input in this case?
        */

        const char *word_str = get_word_string(word_id);
        if (word_str) {
            if (expected_command[0] != '\0') {
                strcat(expected_command, " ");
            }
            strcat(expected_command, word_str);
        }
    }

    if (expected_command[0] == '\0') return 0;

    /* Build semantic matching prompt */
    snprintf(prompt, sizeof(prompt),
        "<|system|>\n"
        "You are a command matcher. Determine if user input in any language matches a game command in English.\n"
        "Answer ONLY 'yes' or 'no'.</s>\n"
        "<|user|>\n"
        "Expected command: %s\n"
        "User input: %s\n"
        "</s>\n"
        "<|assistant|>\n",
        expected_command, input);

    /* Tokenize */
    n_tokens = llama_n_ctx(g_llm_state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(g_llm_state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return 0;
    }

    /* Process prompt */
    struct llama_batch batch = llama_batch_init(g_llm_config.batch_size, 0, 1);
    for (int i = 0; i < n_prompt_tokens; i += g_llm_config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > g_llm_config.batch_size) n_eval = g_llm_config.batch_size;

        batch.n_tokens = n_eval;
        for (int k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = 0;
            batch.logits[k] = false;
        }
        if (i + n_eval == n_prompt_tokens) {
            batch.logits[n_eval - 1] = true;
        }
        if (llama_decode(g_llm_state->ctx, batch) != 0) {
            llama_batch_free(batch);
            free(tokens);
            return 0;
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Generate response */
    int response_len = 0;
    struct llama_batch batch_gen = llama_batch_init(1, 0, 1);
    while (response_len < (int)sizeof(response_buf) - 1) {
        llama_token new_token = llama_sampler_sample(g_llm_state->sampler, g_llm_state->ctx, -1);
        llama_sampler_accept(g_llm_state->sampler, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(g_llm_state->model), new_token)) break;

        char piece[64];
        int piece_len = llama_token_to_piece(llama_model_get_vocab(g_llm_state->model),
                                             new_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < (int)sizeof(response_buf) - 1) {
            memcpy(response_buf + response_len, piece, piece_len);
            response_len += piece_len;
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + response_len;
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = 0;
        batch_gen.logits[0] = true;

        if (llama_decode(g_llm_state->ctx, batch_gen) != 0) break;
    }
    llama_batch_free(batch_gen);
    response_buf[response_len] = '\0';

    /* Normalize response */
    for (int i = 0; i < response_len; i++) {
        response_buf[i] = tolower((unsigned char)response_buf[i]);
    }

    if (strstr(response_buf, "yes")) 
        return 1; // TODO: clean the user input in this case?
    if (strstr(response_buf, "no")) 
        return 0;

    return 0; /* Default: no match */
}
