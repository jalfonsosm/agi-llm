/*
 * nagi_llm_bitnet.h - BitNet.cpp Backend for NAGI-LLM
 *
 * This backend uses BitNet.cpp for 1.58-bit quantized model inference.
 * BitNet models are extremely efficient and fast on CPU.
 */

#ifndef NAGI_LLM_BITNET_H
#define NAGI_LLM_BITNET_H

#include "../../include/nagi_llm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a BitNet.cpp backend instance
 * Called internally by nagi_llm_create(NAGI_LLM_BACKEND_BITNET)
 */
nagi_llm_t *nagi_llm_bitnet_create(void);

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_BITNET_H */
