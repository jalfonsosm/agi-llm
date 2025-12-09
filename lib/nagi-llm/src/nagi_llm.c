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

/* Forward declarations for backend constructors */
#ifdef NAGI_LLM_HAS_LLAMACPP
nagi_llm_t *nagi_llm_llamacpp_create(void);
#endif

#ifdef NAGI_LLM_HAS_BITNET
nagi_llm_t *nagi_llm_bitnet_create(void);
#endif

#ifdef NAGI_LLM_HAS_CLOUD_API
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

#ifdef NAGI_LLM_HAS_CLOUD_API
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

    //nagi_llm_init(llm, NULL, NULL);

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
    if (!llm || !llm->init) {
        return 0;
    }
    return llm->init(llm, model_path, config);
}

/*
 * Shutdown the LLM parser
 */
void nagi_llm_shutdown(nagi_llm_t *llm) {
    if (!llm || !llm->shutdown) return;
    llm->shutdown(llm);
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
    if (!llm || !llm->extract_words) return input;
    return llm->extract_words(llm, input);
}

int nagi_llm_matches_expected(nagi_llm_t *llm, const char *input,
                                    const int *expected_word_ids, int expected_count) {
    return llm && llm->matches_expected ? llm->matches_expected(llm, input, expected_word_ids, expected_count) : 0;
}

int nagi_llm_generate_response(nagi_llm_t *llm, const char *game_response,
                                      const char *user_input, char *output, int output_size) {
    return llm && llm->generate_response ? llm->generate_response(llm, game_response, user_input, output, output_size) : 0;
}
