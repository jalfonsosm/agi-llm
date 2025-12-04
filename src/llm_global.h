/*
 * llm_global.h - Global LLM Instance for NAGI
 *
 * This header provides access to the global LLM instance and configuration.
 * The LLM is initialized in initialise.c and used throughout the codebase.
 */

#ifndef NAGI_LLM_GLOBAL_H
#define NAGI_LLM_GLOBAL_H

#ifdef NAGI_ENABLE_LLM
#include <nagi_llm.h>
#include <nagi_llm_context.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global LLM instance */
extern nagi_llm_t *g_llm;

/* Global LLM configuration - exposed for mode checking in parse.c and logic_eval.c */
extern nagi_llm_config_t g_llm_config;

#ifdef __cplusplus
}
#endif

#endif /* NAGI_ENABLE_LLM */

#endif /* NAGI_LLM_GLOBAL_H */
