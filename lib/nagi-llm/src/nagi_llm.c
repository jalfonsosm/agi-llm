/*
 * nagi_llm.c - Abstract LLM interface implementation
 */

#include "../include/nagi_llm.h"
#include <stdlib.h>
#include <string.h>

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
        /* Initialize config with defaults */
        memset(&llm->config, 0, sizeof(llm->config));
        llm->config.backend = backend;
        llm->config.context_size = NAGI_LLM_DEFAULT_CONTEXT_SIZE;
        llm->config.batch_size = NAGI_LLM_DEFAULT_BATCH_SIZE;
        llm->config.u_batch_size = NAGI_LLM_DEFAULT_U_BATCH_SIZE;
        llm->config.n_threads = NAGI_LLM_DEFAULT_THREADS;
        llm->config.temperature = 0.0f;  /* Greedy decoding by default */
        llm->config.top_p = 0.9f;
        llm->config.top_k = 1;
        llm->config.max_tokens = 5;
        llm->config.use_gpu = 1;
        llm->config.verbose = 0;
        llm->config.mode = NAGI_LLM_MODE_EXTRACTION;
    }

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

    /* Call backend shutdown if initialized */
    if (llm->shutdown) {
        llm->shutdown(llm);
    }

    /* Free the instance */
    free(llm);
}
