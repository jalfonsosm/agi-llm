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
