/*
 * nagi_llm.c - Abstract LLM interface implementation
 */

#include "../include/nagi_llm.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "llama.h"

/* Forward declarations for backend constructors */
#ifdef NAGI_LLM_HAS_LLAMACPP
nagi_llm_t *nagi_llm_llamacpp_create(void);
#endif

#ifdef NAGI_LLM_HAS_BITNET
nagi_llm_t *nagi_llm_bitnet_create(void);
#endif

#ifdef NAGI_LLM_HAS_CLOUD_API
nagi_llm_t *nagi_llm_cloud_api_create(void);
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

#ifdef NAGI_LLM_HAS_CLOUD_API
        case NAGI_LLM_BACKEND_CLOUD_API:
            llm = nagi_llm_cloud_api_create();
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
    // ctx_params.flash_attn = llm->config.flash_attn;
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

    /* Create sampler for extraction/semantic (deterministic) */
    state->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_k(llm->config.top_k));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_p(llm->config.top_p, 1));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_temp(llm->config.temperature));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_dist(42));

    /* Create sampler for response generation (creative) */
    state->sampler_creative = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_dist(42));

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

const char *nagi_llm_extract_words(nagi_llm_t *llm, const char *input) {
    return llm && llm->extract_words ? llm->extract_words(llm, input) : NULL;
}

int nagi_llm_matches_expected(nagi_llm_t *llm, const char *input,
                                    const int *expected_word_ids, int expected_count) {
    return llm && llm->matches_expected ? llm->matches_expected(llm, input, expected_word_ids, expected_count) : 0;
}

int nagi_llm_generate_response(nagi_llm_t *llm, const char *game_response,
                                      const char *user_input, char *output, int output_size) {
    return llm && llm->generate_response ? llm->generate_response(llm, game_response, user_input, output, output_size) : 0;
}
