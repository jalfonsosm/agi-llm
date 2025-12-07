#ifndef NAGI_LLM_CLOUD_H
#define NAGI_LLM_CLOUD_H

#include "../../include/nagi_llm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char api_url[512];
    char api_key[256];
    char model[128];
    float temperature;
    int max_tokens;
} nagi_llm_cloud_config_t;

int nagi_llm_cloud_init(nagi_llm_t *llm, const nagi_llm_cloud_config_t *config);
int nagi_llm_cloud_generate(nagi_llm_t *llm, const char *prompt, char *output, int output_size);
void nagi_llm_cloud_cleanup(nagi_llm_t *llm);

#ifdef __cplusplus
}
#endif

#endif
