/*
 * nagi_llm_bitnet.c - BitNet.cpp Backend Implementation (REAL)
 *
 * BitNet.cpp uses llama.cpp API with optimized 1.58-bit quantized kernels
 * Performance: 2-5x faster than standard llama.cpp, 4-5x less memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "nagi_llm_bitnet.h"
#include "../../include/nagi_llm_context.h"

/* BitNet uses llama.cpp API */
#include "llama.h"

/* Basic types */
typedef unsigned char u8;
typedef unsigned short u16;

/* Local endian conversion */
static inline u16 load_be_16(const u8 *ptr) {
    return (u16)((ptr[0] << 8) | ptr[1]);
}

/* Extraction prompt template */
static const char *EXTRACTION_PROMPT_TEMPLATE =
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: mira el castillo<|end|>\n"
    "<|assistant|>\n"
    "look castle<|end|>\n"
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: %s<|end|>\n"
    "<|assistant|>\n";

static const char *EXTRACTION_PROMPT_SIMPLE =
    "<|user|>\n"
    "Translate to English (verb noun only):\n"
    "%s<|end|>\n"
    "<|assistant|>\n";

/* Internal backend state */
typedef struct bitnet_state {
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;

    int initialized;
    char last_error[256];
    int seq_counter;

    const u8 *dictionary_data;
    size_t dictionary_size;
} bitnet_state_t;

static bitnet_state_t *g_bitnet_state = NULL;
static nagi_llm_config_t g_bitnet_config = {
    .backend = NAGI_LLM_BACKEND_BITNET,
    .model_path = "",
    .context_size = 2048,  /* Smaller context for BitNet efficiency */
    .batch_size = 512,
    .u_batch_size = 256,
    .n_threads = 2,  /* BitNet is CPU-optimized, fewer threads often faster */
    .temperature = 0.0f,
    .top_p = 0.9f,
    .top_k = 1,
    .max_tokens = 5,
    .use_gpu = 0,  /* BitNet is pure CPU */
    .verbose = 0,
    .mode = NAGI_LLM_MODE_EXTRACTION
};

static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (g_bitnet_state) {
        vsnprintf(g_bitnet_state->last_error, sizeof(g_bitnet_state->last_error), fmt, args);
    } else {
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        fprintf(stderr, "BitNet Error: %s\n", buf);
    }
    va_end(args);
}

/* Extract game verbs from dictionary */
static const char *extract_game_verbs(void)
{
    static char verbs_buf[512];
    const u8 *dict_data;
    u16 offsets[26];
    int verb_count;
    int i, j;

    if (!g_bitnet_state || !g_bitnet_state->dictionary_data) {
        return NULL;
    }

    dict_data = g_bitnet_state->dictionary_data;

    /* Read 26 letter offsets (52 bytes header) */
    for (i = 0; i < 26; i++) {
        offsets[i] = load_be_16(dict_data + i * 2);
    }

    /* Extract verbs (first ~15 words are typically verbs in AGI) */
    verbs_buf[0] = '\0';
    verb_count = 0;

    for (i = 0; i < 26 && verb_count < 15; i++) {
        u16 offset = offsets[i];
        if (offset == 0 || offset >= g_bitnet_state->dictionary_size) continue;

        const u8 *word_data = dict_data + offset;

        for (j = 0; j < 100 && verb_count < 15; j++) {
            u16 word_id = load_be_16(word_data);
            word_data += 2;

            if (word_id == 0) break;

            u8 word_len = *word_data++;
            if (word_len == 0 || word_len > 64) break;

            if (verb_count > 0) {
                strcat(verbs_buf, ", ");
            }

            strncat(verbs_buf, (const char *)word_data, word_len);
            word_data += word_len;
            verb_count++;
        }
    }

    return verbs_buf[0] != '\0' ? verbs_buf : NULL;
}

/*
 * Initialize BitNet backend
 */
static int bitnet_init(nagi_llm_t *self, const char *model_path, const nagi_llm_config_t *config)
{
    struct llama_model_params model_params;
    struct llama_context_params ctx_params;

    (void)self;

    if (g_bitnet_state && g_bitnet_state->initialized) {
        fprintf(stderr, "BitNet: Already initialized\n");
        return 1;
    }

    if (!g_bitnet_state) {
        g_bitnet_state = (bitnet_state_t *)malloc(sizeof(bitnet_state_t));
        if (!g_bitnet_state) {
            set_error("Failed to allocate state");
            return 0;
        }
        memset(g_bitnet_state, 0, sizeof(*g_bitnet_state));
    }

    if (config) {
        memcpy(&g_bitnet_config, config, sizeof(nagi_llm_config_t));
    }

    if (model_path && model_path[0] != '\0') {
        strncpy(g_bitnet_config.model_path, model_path, NAGI_LLM_MAX_MODEL_PATH - 1);
        g_bitnet_config.model_path[NAGI_LLM_MAX_MODEL_PATH - 1] = '\0';
    }

    if (g_bitnet_config.verbose) {
        fprintf(stderr, "BitNet: Initializing with model: %s\n", g_bitnet_config.model_path);
    }

    /* Initialize llama.cpp backend */
    llama_backend_init();

    /* Load model with BitNet optimizations */
    model_params = llama_model_default_params();
    model_params.use_mmap = true;
    model_params.use_mlock = false;

    g_bitnet_state->model = llama_model_load_from_file(g_bitnet_config.model_path, model_params);
    if (!g_bitnet_state->model) {
        set_error("Failed to load model: %s", g_bitnet_config.model_path);
        free(g_bitnet_state);
        g_bitnet_state = NULL;
        return 0;
    }

    /* Create context */
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = g_bitnet_config.context_size;
    ctx_params.n_batch = g_bitnet_config.batch_size;
    ctx_params.n_ubatch = g_bitnet_config.u_batch_size;
    ctx_params.n_threads = g_bitnet_config.n_threads;
    ctx_params.n_threads_batch = g_bitnet_config.n_threads;
    ctx_params.flash_attn = false;  /* BitNet works best without flash attention */

    g_bitnet_state->ctx = llama_new_context_with_model(g_bitnet_state->model, ctx_params);
    if (!g_bitnet_state->ctx) {
        set_error("Failed to create context");
        llama_model_free(g_bitnet_state->model);
        free(g_bitnet_state);
        g_bitnet_state = NULL;
        return 0;
    }

    /* Create sampler (greedy for deterministic extraction) */
    struct llama_sampler_chain_params sampler_params = llama_sampler_chain_default_params();
    g_bitnet_state->sampler = llama_sampler_chain_init(sampler_params);

    llama_sampler_chain_add(g_bitnet_state->sampler,
                           llama_sampler_init_temp(g_bitnet_config.temperature));
    llama_sampler_chain_add(g_bitnet_state->sampler,
                           llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    g_bitnet_state->initialized = 1;
    g_bitnet_state->seq_counter = 0;

    if (g_bitnet_config.verbose) {
        fprintf(stderr, "BitNet: Initialized successfully\n");
        fprintf(stderr, "BitNet: Context size: %d, Threads: %d\n",
                g_bitnet_config.context_size, g_bitnet_config.n_threads);
    }

    return 1;
}

/*
 * Shutdown BitNet backend
 */
static void bitnet_shutdown(nagi_llm_t *self)
{
    (void)self;

    if (!g_bitnet_state) return;

    if (g_bitnet_config.verbose) {
        fprintf(stderr, "BitNet: Shutting down\n");
    }

    if (g_bitnet_state->sampler) {
        llama_sampler_free(g_bitnet_state->sampler);
    }
    if (g_bitnet_state->ctx) {
        llama_context_free(g_bitnet_state->ctx);
    }
    if (g_bitnet_state->model) {
        llama_model_free(g_bitnet_state->model);
    }

    llama_backend_free();

    free(g_bitnet_state);
    g_bitnet_state = NULL;
}

/*
 * Check if ready
 */
static int bitnet_ready(nagi_llm_t *self)
{
    (void)self;
    return g_bitnet_state && g_bitnet_state->initialized;
}

/*
 * Extract words from input
 */
static const char *bitnet_extract_words(nagi_llm_t *self, const char *input)
{
    static char response_buf[256];
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    char piece[64];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_extract_tokens;
    llama_token *tokens;
    int i, k, n_eval;
    int piece_len;

    (void)self;

    if (!bitnet_ready(self)) return input;
    if (!input || input[0] == '\0') return input;

    /* Build prompt */
    const char *verbs = extract_game_verbs();
    if (verbs && verbs[0] != '\0') {
        snprintf(prompt, sizeof(prompt), EXTRACTION_PROMPT_TEMPLATE, verbs, verbs, input);
    } else {
        snprintf(prompt, sizeof(prompt), EXTRACTION_PROMPT_SIMPLE, input);
    }

    /* Use rotating sequences */
    current_seq = (g_bitnet_state->seq_counter++) % 4;

    /* Clear KV cache */
    llama_memory_t mem = llama_get_memory(g_bitnet_state->ctx);
    llama_memory_seq_rm(mem, current_seq, -1, -1);

    /* Tokenize */
    n_tokens = llama_n_ctx(g_bitnet_state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(g_bitnet_state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return input;
    }

    /* Process prompt */
    struct llama_batch batch = llama_batch_init(g_bitnet_config.batch_size, 0, 4);
    for (i = 0; i < n_prompt_tokens; i += g_bitnet_config.batch_size) {
        n_eval = n_prompt_tokens - i;
        if (n_eval > g_bitnet_config.batch_size) n_eval = g_bitnet_config.batch_size;

        batch.n_tokens = n_eval;
        for (k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = current_seq;
            batch.logits[k] = false;
        }
        if (i + n_eval == n_prompt_tokens) {
            batch.logits[n_eval - 1] = true;
        }

        if (llama_decode(g_bitnet_state->ctx, batch) != 0) {
            llama_batch_free(batch);
            free(tokens);
            return input;
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Generate response */
    response_len = 0;
    gen_count = 0;
    max_extract_tokens = 10;

    struct llama_batch batch_gen = llama_batch_init(1, 0, 4);

    while (response_len < (int)sizeof(response_buf) - 1 && gen_count < max_extract_tokens) {
        llama_token new_token = llama_sampler_sample(g_bitnet_state->sampler, g_bitnet_state->ctx, -1);
        llama_sampler_accept(g_bitnet_state->sampler, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(g_bitnet_state->model), new_token)) {
            break;
        }

        piece_len = llama_token_to_piece(llama_model_get_vocab(g_bitnet_state->model),
                                        new_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < (int)sizeof(response_buf) - 1) {
            memcpy(response_buf + response_len, piece, piece_len);
            response_len += piece_len;

            if (strchr(piece, '\n') != NULL) break;
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + gen_count;
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = current_seq;
        batch_gen.logits[0] = true;

        if (llama_decode(g_bitnet_state->ctx, batch_gen) != 0) {
            break;
        }
        gen_count++;
    }

    llama_batch_free(batch_gen);
    response_buf[response_len] = '\0';

    /* Trim and lowercase */
    char *trimmed = response_buf;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') {
        trimmed++;
    }
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
        *end-- = '\0';
    }

    for (i = 0; trimmed[i]; i++) {
        trimmed[i] = tolower((unsigned char)trimmed[i]);
    }

    if (trimmed != response_buf) {
        memmove(response_buf, trimmed, strlen(trimmed) + 1);
    }

    return response_buf;
}

/*
 * Semantic matching
 */
static int bitnet_matches_expected(nagi_llm_t *self, const char *input,
                                   const int *expected_word_ids, int expected_count)
{
    const char *extracted;
    char *extracted_copy;
    char *token;
    int i, j;
    int match_count = 0;
    const u8 *dict_data;
    
    if (!bitnet_ready(self) || !input || !expected_word_ids || expected_count <= 0) {
        return 0;
    }

    /* 1. Extract words from input (e.g., "look castle") */
    extracted = bitnet_extract_words(self, input);
    if (!extracted || extracted[0] == '\0') {
        return 0;
    }

    /* 2. Check if extracted words match any expected words */
    /* We need to look up the string representation of expected_word_ids */
    if (!g_bitnet_state->dictionary_data) {
        return 0;
    }
    dict_data = g_bitnet_state->dictionary_data;

    /* Make a copy of extracted string to tokenize */
    extracted_copy = strdup(extracted);
    if (!extracted_copy) return 0;

    token = strtok(extracted_copy, " ,");
    while (token) {
        int token_matched = 0;
        
        /* Check this token against all expected words */
        for (i = 0; i < expected_count; i++) {
            int expected_id = expected_word_ids[i];
            
            /* Look up word string in dictionary */
            /* Dictionary format: 26 offsets (52 bytes) -> word nodes */
            /* We have to scan the whole dictionary to find the ID? That's slow. */
            /* AGI dictionary is optimized for string -> ID, not ID -> string. */
            /* However, we can optimize by caching or just scanning since dict is small (~20KB) */

            /* Scan dictionary for this ID */
            for (j = 0; j < 26; j++) {
                u16 offset = load_be_16(dict_data + j * 2);
                if (offset == 0 || offset >= g_bitnet_state->dictionary_size) continue;

                const u8 *word_node = dict_data + offset;
                while (1) {
                    u16 id = load_be_16(word_node);
                    word_node += 2;
                    if (id == 0) break; /* End of list for this letter */

                    u8 len = *word_node++;
                    if (len == 0) break; /* Should not happen if id != 0 */
                    
                    /* Check if this is the word we are looking for */
                    if (id == expected_id) {
                        /* Compare with extracted token (check length first for safety) */
                        size_t token_len = strlen(token);
                        if (token_len == len &&
                            strncasecmp(token, (const char *)word_node, len) == 0) {
                            token_matched = 1;
                        }
                    }
                    
                    word_node += len;
                    if (token_matched) break;
                }
                if (token_matched) break;
            }
            if (token_matched) break;
        }
        
        if (token_matched) {
            match_count++;
        }
        
        token = strtok(NULL, " ,");
    }

    free(extracted_copy);

    /* If we matched at least one significant word, consider it a match */
    /* Adjust logic as needed - maybe we need to match ALL extracted words? */
    /* For now, if we found any of the expected words in the extraction, return true */
    return match_count > 0;
}

/*
 * Generate response (simplified for BitNet)
 */
static int bitnet_generate_response(nagi_llm_t *self, const char *game_response,
                                   const char *user_input, char *output, int output_size)
{
    (void)self;
    (void)user_input;

    if (!bitnet_ready(self)) return 0;
    if (!game_response || !output || output_size <= 0) return 0;

    /* BitNet optimized for extraction, not generation - just return game response */
    strncpy(output, game_response, output_size - 1);
    output[output_size - 1] = '\0';

    return (int)strlen(output);
}

/*
 * Set dictionary
 */
static int bitnet_set_dictionary(nagi_llm_t *self, const unsigned char *dictionary, size_t size)
{
    (void)self;

    if (!g_bitnet_state) {
        fprintf(stderr, "BitNet: Cannot set dictionary - not initialized\n");
        return 0;
    }

    g_bitnet_state->dictionary_data = dictionary;
    g_bitnet_state->dictionary_size = size;

    if (g_bitnet_config.verbose) {
        fprintf(stderr, "BitNet: Dictionary set (%zu bytes)\n", size);
    }

    return 1;
}

/*
 * Create BitNet backend instance
 */
nagi_llm_t *nagi_llm_bitnet_create(void)
{
    nagi_llm_t *llm;

    llm = (nagi_llm_t *)malloc(sizeof(nagi_llm_t));
    if (!llm) {
        return NULL;
    }

    memset(llm, 0, sizeof(*llm));

    /* Set up vtable */
    llm->backend = NAGI_LLM_BACKEND_BITNET;
    llm->init = bitnet_init;
    llm->shutdown = bitnet_shutdown;
    llm->ready = bitnet_ready;
    llm->extract_words = bitnet_extract_words;
    llm->matches_expected = bitnet_matches_expected;
    llm->generate_response = bitnet_generate_response;
    llm->set_dictionary = bitnet_set_dictionary;
    llm->impl_data = NULL;

    return llm;
}
