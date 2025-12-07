/*
 * nagi_llm.c - Abstract LLM interface implementation
 */

#include "../include/nagi_llm.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include "../include/llm_utils.h"
#include "llama.h"

/* Forward declarations for backend constructors */
#ifdef NAGI_LLM_HAS_LLAMACPP
nagi_llm_t *nagi_llm_llamacpp_create(void);
#endif

#ifdef NAGI_LLM_HAS_BITNET
nagi_llm_t *nagi_llm_bitnet_create(void);
#endif

#ifdef NAGI_LLM_HAS_CLOUD
nagi_llm_t *nagi_llm_cloud_create(void);
#endif

/* Common error setter */
void set_error(llm_state_t *state, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (state) {
        vsnprintf(state->last_error, sizeof(state->last_error), fmt, args);
    } else {
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        fprintf(stderr, "BitNet Error: %s\n", buf);
    }
    va_end(args);
}

/*
 * Create a new LLM instance for the specified backend
 */
nagi_llm_t *nagi_llm_create(nagi_llm_backend_t backend)
{
    nagi_llm_t *llm = NULL;

    switch (backend) {
#ifdef NAGI_LLM_HAS_LLAMACPP
        case NAGI_LLM_BACKEND_LLAMACPP:
            llm = nagi_llm_llamacpp_create();
            break;
#endif

#ifdef NAGI_LLM_HAS_BITNET
        case NAGI_LLM_BACKEND_BITNET:
            llm = nagi_llm_bitnet_create();
            break;
#endif

#ifdef NAGI_LLM_HAS_CLOUD
        case NAGI_LLM_BACKEND_CLOUD:
            llm = nagi_llm_cloud_create();
            break;
#endif

        default:
            /* Backend not available */
            return NULL;
    }

    if (llm) {
        llm->backend = backend;
    }

    nagi_llm_init(llm, NULL, NULL);

    return llm;
}

/*
 * Destroy an LLM instance
 */
void nagi_llm_destroy(nagi_llm_t *llm)
{
    if (!llm) {
        return;
    }

    nagi_llm_shutdown(llm);

    /* Free the instance */
    free(llm);
}

/*
 * Initialize the LLM backend
 */
int nagi_llm_init(nagi_llm_t *llm, 
                  const char *model_path,
                  const nagi_llm_config_t *config) {
    if (!llm) {
        return 0;
    }

    struct llama_model_params model_params;
    struct llama_context_params ctx_params;
    
    if (llm->state && llm->state->initialized) {
        fprintf(stderr, "BitNet: Already initialized\n");
        return 1;
    }

    if (!llm->state) {
        llm->state = (llm_state_t *)calloc(1, sizeof(llm_state_t));
        if (!llm->state) {
            set_error(NULL, "Failed to allocate state");
            return 0;
        }
    }
    
    llm_state_t *state = llm->state;

    if (config) {
        memcpy(&llm->config, config, sizeof(nagi_llm_config_t));
    }

    if (model_path && model_path[0] != '\0') {
        strncpy(llm->config.model_path, model_path, NAGI_LLM_MAX_MODEL_PATH - 1);
        llm->config.model_path[NAGI_LLM_MAX_MODEL_PATH - 1] = '\0';
    }

    /* Initialize llama.cpp backend */
    llama_backend_init();

    /* Load model */
    model_params = llama_model_default_params();
    
    if (llm->config.use_gpu) {
        model_params.n_gpu_layers = 999;
        model_params.main_gpu = 0;
    } else {
        model_params.n_gpu_layers = 0;
    }
    
    model_params.use_mmap = true;
    model_params.use_mlock = false;

    printf("LLM Parser: Loading model from %s...\n", llm->config.model_path);
    state->model = llama_load_model_from_file(llm->config.model_path, model_params);
    if (!state->model) {
        set_error(state, "Failed to load model: %s", llm->config.model_path);
        fprintf(stderr, "LLM Parser: %s\n", state->last_error);
        free(state);
        state = NULL;
        llm->state = NULL;
        return 0;
    }

    /* Create context */
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = llm->config.context_size;
    ctx_params.n_batch = llm->config.batch_size;
    ctx_params.n_ubatch = llm->config.u_batch_size;
    ctx_params.n_threads = llm->config.n_threads;
    ctx_params.n_threads_batch = llm->config.n_threads;
        
#ifdef NAGI_LLM_HAS_BITNET
    ctx_params.flash_attn = llm->config.flash_attn;
#endif
    
    ctx_params.n_seq_max = llm->config.n_seq_max;

    state->ctx = llama_new_context_with_model(state->model, ctx_params);
    if (!state->ctx) {
        set_error(state, "Failed to create context");
        fprintf(stderr, "LLM Parser: %s\n", state->last_error);
        llama_free_model(state->model);
        free(state);
        llm->state = NULL;
        return 0;
    }

    /* Random seed for variety */
    uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)((uintptr_t)state);
    
    /* Create sampler for extraction/semantic (deterministic) */
    state->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_k(llm->config.top_k));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_p(llm->config.top_p, 1));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_temp(llm->config.temperature));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_dist(seed));

    /* Create sampler for response generation (creative with randomized temperature) */
    float creative_temp = 0.7f + ((float)(seed % 30) / 100.0f);  /* 0.7 to 0.99 */
    state->sampler_creative = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_temp(creative_temp));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_dist(seed + 1));
    
    if (llm->config.verbose) {
        printf("LLM Sampler: seed=%u, creative_temp=%.2f\n", seed, creative_temp);
    }

    state->initialized = 1;
    state->seq_counter = 0;

    if (llm->config.verbose) {
        printf("LLM Parser: Initialized successfully\n");
        printf("  Context size: %d\n", llm->config.context_size);
        printf("  Batch size: %d\n", llm->config.batch_size);
        printf("  Threads: %d\n", llm->config.n_threads);
    }

    return 1;
}

/*
 * Shutdown the LLM parser
 */
void nagi_llm_shutdown(nagi_llm_t *llm) {
    llm_state_t *state = llm->state;

    if (!state) return;

    if (llm->config.verbose) {
        fprintf(stderr, "BitNet: Shutting down\n");
    }

    if (state->sampler) {
        llama_sampler_free(state->sampler);
    }
    if (state->sampler_creative) {
        llama_sampler_free(state->sampler_creative);
    }
    if (state->ctx) {
        llama_free(state->ctx);
    }
    if (state->model) {
        llama_free_model(state->model);
    }

    llama_backend_free();

    free(state);
    llm->state = NULL;

    if (llm->config.verbose) {
        printf("LLM Parser: Shutdown complete\n");
    }
}

/*
 * Check if LLM is initialized and ready
 */
inline int nagi_llm_ready(nagi_llm_t *llm) {
    llm_state_t *state = llm->state;
    return state && state->initialized;
}

/*
 * Set dictionary
 */
int nagi_llm_set_dictionary(nagi_llm_t *llm, const unsigned char *dictionary, size_t size) {
    llm_state_t *state = llm->state;

    if (!state) {
        fprintf(stderr, "LLM Parser: Cannot set dictionary - not initialized\n");
        return 0;
    }

    state->dictionary_data = dictionary;
    state->dictionary_size = size;

    if (llm->config.verbose) {
        fprintf(stderr, "LLM Parser: Dictionary set (%zu bytes)\n", size);
    }

    return 1;
}

/*
 * Extract verb and noun from user input (EXTRACTION mode)
 * Translates "mira el castillo" -> "look castle"
 * Much faster than semantic matching, uses shorter prompt
 * Returns static buffer with extracted English words
 */
const char *nagi_llm_extract_words(nagi_llm_t *llm, const char *input) {
    static char response_buf[NAGI_LLM_MAX_RESPONSE_SIZE];
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    char piece[64];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_extract_tokens;
    llama_token *tokens;

    if (!nagi_llm_ready(llm)) return input;
    if (!input || input[0] == '\0') return input;

    /* Extract game verbs for vocabulary hint */
    const char *verbs = extract_game_verbs(llm);

    /* Build extraction prompt with vocabulary context */
    if (verbs && verbs[0] != '\0' && llm->extraction_prompt_template) {
        /* Use template with verb vocabulary */
        snprintf(prompt, sizeof(prompt), llm->extraction_prompt_template,
                 verbs, verbs, verbs, input);
    } else if (llm->extraction_prompt_simple) {
        /* Fallback to simple prompt */
        snprintf(prompt, sizeof(prompt), llm->extraction_prompt_simple, input);
    } else {
        /* No prompts available - should not happen */
        return input;
    }

    llm_state_t *state = llm->state;

    /* Use rotating sequence IDs to avoid KV cache conflicts */
    current_seq = (state->seq_counter++) % 8;

    if (llm->config.verbose) {
        printf("\n=== LLM Extraction ===\n");
        printf("Input: \"%s\"\n", input);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache for this sequence */
#ifdef NAGI_LLM_HAS_BITNET
    llama_kv_cache_seq_rm(state->ctx, current_seq, -1, -1);
#else
    llama_memory_t mem = llama_get_memory(state->ctx);
    llama_memory_seq_rm(mem, current_seq, -1, -1);
#endif

    /* Tokenize prompt */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
#ifdef NAGI_LLM_HAS_BITNET
    n_prompt_tokens = llama_tokenize(state->model,
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
#else
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
#endif
    if (n_prompt_tokens < 0) {
        free(tokens);
        return input;
    }

    /* Process prompt in batches */
    struct llama_batch batch = llama_batch_init(llm->config.batch_size, 0, 8);
    for (int i = 0; i < n_prompt_tokens; i += llm->config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > llm->config.batch_size) n_eval = llm->config.batch_size;

        batch.n_tokens = n_eval;
        for (int k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = current_seq;
            batch.logits[k] = false;
        }
        if (i + n_eval == n_prompt_tokens) {
            batch.logits[n_eval - 1] = true;
        }

        if (llama_decode(state->ctx, batch) != 0) {
            llama_batch_free(batch);
            free(tokens);
            return input;
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Generate response (extract English words) */
    response_len = 0;
    gen_count = 0;
    max_extract_tokens = 10;  // Short limit - we only need "verb noun"
    struct llama_batch batch_gen = llama_batch_init(1, 0, 8);

    while (response_len < (int)sizeof(response_buf) - 1 && gen_count < max_extract_tokens) {
        llama_token new_token = llama_sampler_sample(state->sampler, state->ctx, -1);
        llama_sampler_accept(state->sampler, new_token);

#ifdef NAGI_LLM_HAS_BITNET
        if (llama_token_is_eog(state->model, new_token)) {
#else
        if (llama_vocab_is_eog(llama_model_get_vocab(state->model), new_token)) {
#endif
            break;
        }

#ifdef NAGI_LLM_HAS_BITNET
        int piece_len = llama_token_to_piece(state->model,
                                             new_token, piece, sizeof(piece), 0, true);
#else
        int piece_len = llama_token_to_piece(llama_model_get_vocab(state->model),
                                             new_token, piece, sizeof(piece), 0, true);
#endif
        if (piece_len > 0 && response_len + piece_len < (int)sizeof(response_buf) - 1) {
            memcpy(response_buf + response_len, piece, piece_len);
            response_len += piece_len;

            /* Stop if we hit a newline - extraction should be one line only */
            if (strchr(piece, '\n') != NULL) {
                break;
            }
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + gen_count;
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = current_seq;
        batch_gen.logits[0] = true;

        if (llama_decode(state->ctx, batch_gen) != 0) {
            break;
        }
        gen_count++;
    }

    llama_batch_free(batch_gen);
    response_buf[response_len] = '\0';

    /* Normalize: trim whitespace and lowercase */
    char *trimmed = response_buf;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') {
        trimmed++;
    }
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
        *end-- = '\0';
    }

    /* Convert to lowercase */
    for (int i = 0; trimmed[i]; i++) {
        trimmed[i] = tolower((unsigned char)trimmed[i]);
    }

    if (llm->config.verbose) {
        printf("Extracted: \"%s\"\n", trimmed);
        printf("===================\n\n");
    }

    /* Copy trimmed result back to start of buffer */
    if (trimmed != response_buf) {
        memmove(response_buf, trimmed, strlen(trimmed) + 1);
    }

    return response_buf;
}

int nagi_llm_matches_expected(nagi_llm_t *llm, const char *input,
                                    const int *expected_word_ids, int expected_count) {
    return llm && llm->matches_expected ? llm->matches_expected(llm, input, expected_word_ids, expected_count) : 0;
}

int nagi_llm_generate_response(nagi_llm_t *llm, const char *game_response,
                                      const char *user_input, char *output, int output_size) {
    return llm && llm->generate_response ? llm->generate_response(llm, game_response, user_input, output, output_size) : 0;
}
