/*
 * nagi_llm_llamacpp.h - llama.cpp Backend Implementation
 */

#ifndef NAGI_LLM_LLAMACPP_H
#define NAGI_LLM_LLAMACPP_H

#include "../../include/nagi_llm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a llama.cpp backend instance
 * Called internally by nagi_llm_create(NAGI_LLM_BACKEND_LLAMACPP)
 */
nagi_llm_t *nagi_llm_llamacpp_create(void);

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_LLAMACPP_H */
