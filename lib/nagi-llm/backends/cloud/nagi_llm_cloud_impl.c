#include "nagi_llm_cloud.h"
#include "../../include/llm_utils.h"
#include "../../include/nagi_llm_context.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

static int cloud_matches_expected(nagi_llm_t *llm, const char *input,
                                   const int *expected_word_ids, int expected_count);
static int cloud_generate_response(nagi_llm_t *llm, const char *game_response,
                                    const char *user_input, char *output, int output_size);

static int cloud_init(nagi_llm_t *llm, const char *model_path, const nagi_llm_config_t *config) {
    (void)model_path;
    /* Override with passed config if provided */
    if (config) {
        llm->config.temperature = config->temperature;
        llm->config.temperature_creative_base = config->temperature_creative_base;
        llm->config.temperature_creative_offset = config->temperature_creative_offset;
        llm->config.max_tokens = config->max_tokens;
        llm->config.verbose = config->verbose;
    }
    
    if (!llm->state) {
        llm->state = (llm_state_t *)calloc(1, sizeof(llm_state_t));
        if (!llm->state) return 0;
    }
    llm->state->initialized = 1;
    
    /* Calculate randomized creative temperature */
    srand((unsigned int)time(NULL));
    float creative_temp = llm->config.temperature_creative_base + 
                         ((float)(rand() % 100) / 100.0f) * llm->config.temperature_creative_offset;
    
    nagi_llm_cloud_config_t cloud_config = {
        .api_url = "",
        .api_key = "",
        .model = "",
        .temperature = creative_temp,  /* Use randomized creative temperature */
        .max_tokens = llm->config.max_tokens
    };
    
    /* Copy from unified config */
    strncpy(cloud_config.api_url, llm->config.api_endpoint, sizeof(cloud_config.api_url) - 1);
    strncpy(cloud_config.api_key, llm->config.api_key, sizeof(cloud_config.api_key) - 1);
    strncpy(cloud_config.model, llm->config.model_path, sizeof(cloud_config.model) - 1);
    
    /* Fallback to environment variable if no API key */
    if (!cloud_config.api_key[0]) {
        const char *api_key = getenv("OPENAI_API_KEY");
        if (api_key && api_key[0]) {
            strncpy(cloud_config.api_key, api_key, sizeof(cloud_config.api_key) - 1);
        }
    }
    
    if (!cloud_config.api_key[0]) {
        fprintf(stderr, "Cloud LLM: No API key found. Set api_key in llm_config.ini or OPENAI_API_KEY env var\n");
        free(llm->state);
        llm->state = NULL;
        return 0;
    }
    
    if (nagi_llm_cloud_init(llm, &cloud_config) != 0) {
        free(llm->state);
        llm->state = NULL;
        return 0;
    }
    
    return 1;
}

static void cloud_shutdown(nagi_llm_t *llm) {
    nagi_llm_cloud_cleanup(llm);
    if (llm->state) {
        free(llm->state);
        llm->state = NULL;
    }
}

static const char *cloud_extract_words(nagi_llm_t *llm, const char *input) {
    static char response_buf[NAGI_LLM_MAX_RESPONSE_SIZE];
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    
    if (!input || input[0] == '\0') return input;
    
    /* Extract game verbs for vocabulary hint */
    const char *verbs = extract_game_verbs(llm);
    
    /* Build extraction prompt */
    if (verbs && verbs[0] != '\0' && llm->extraction_prompt_template) {
        snprintf(prompt, sizeof(prompt), llm->extraction_prompt_template,
                 verbs, verbs, verbs, input);
    } else if (llm->extraction_prompt_simple) {
        snprintf(prompt, sizeof(prompt), llm->extraction_prompt_simple, input);
    } else {
        return input;
    }
    
    int len = nagi_llm_cloud_generate(llm, prompt, response_buf, sizeof(response_buf));
    if (len <= 0) return input;
    
    /* Trim and lowercase */
    char *trimmed = response_buf;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') trimmed++;
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) *end-- = '\0';
    
    for (char *p = trimmed; *p; p++) *p = tolower((unsigned char)*p);
    
    if (trimmed != response_buf) memmove(response_buf, trimmed, strlen(trimmed) + 1);
    
    return response_buf;
}

nagi_llm_t *nagi_llm_cloud_create(void) {
    nagi_llm_t *llm = (nagi_llm_t *)calloc(1, sizeof(nagi_llm_t));
    if (!llm) return NULL;
    
    llm->backend = NAGI_LLM_BACKEND_CLOUD;
    llm->init = cloud_init;
    llm->shutdown = cloud_shutdown;
    llm->extract_words = cloud_extract_words;
    llm->matches_expected = cloud_matches_expected;
    llm->generate_response = cloud_generate_response;
    llm->extraction_prompt_template = EXTRACTION_PROMPT_TEMPLATE;
    llm->extraction_prompt_simple = EXTRACTION_PROMPT_SIMPLE;
    
    /* Set default temperature values */
    llm->config.temperature = 0.0f;  /* Extraction temperature (deterministic) */
    llm->config.temperature_creative_base = 0.3f;
    llm->config.temperature_creative_offset = 0.2f;
    llm->config.max_tokens = 512;
    llm->config.verbose = 0;
    
    return llm;
}

static int cloud_matches_expected(nagi_llm_t *llm, const char *input,
                                   const int *expected_word_ids, int expected_count) {
    char expected_str[256] = {0};
    char prompt[2048];
    char response[128];
    
    for (int i = 0; i < expected_count; i++) {
        const char *word = get_word_string(llm, expected_word_ids[i]);
        if (word) {
            if (i > 0) strcat(expected_str, " ");
            strcat(expected_str, word);
        }
    }
    
    snprintf(prompt, sizeof(prompt), SEMANTIC_MATCHING_PROMPT, expected_str, input);
    
    int len = nagi_llm_cloud_generate(llm, prompt, response, sizeof(response));
    if (len <= 0) return 0;
    
    return (strstr(response, "yes") != NULL);
}

static int cloud_generate_response(nagi_llm_t *llm, const char *game_response,
                                    const char *user_input, char *output, int output_size) {
    char prompt[4096];
    char lang_prompt[512];
    char detected[64];
    llm_state_t *state = llm->state;
    const char *language = "English";
    
    /* Detect language from user input using API */
    if (user_input && user_input[0] != '\0') {
        snprintf(lang_prompt, sizeof(lang_prompt), LANGUAGE_DETECTION_PROMPT, user_input);
        int len = nagi_llm_cloud_generate(llm, lang_prompt, detected, sizeof(detected));
        
        if (len > 0) {
            /* Trim whitespace */
            char *p = detected;
            while (*p == ' ' || *p == '\n') p++;
            char *end = p + strlen(p) - 1;
            while (end > p && (*end == ' ' || *end == '\n' || *end == '.')) *end-- = '\0';
            
            if (strlen(p) > 2 && strlen(p) < 32) {
                language = p;
                if (state) strcpy(state->detected_language, p);
            }
        }
    } else if (state && state->detected_language[0]) {
        language = state->detected_language;
    }

    if (llm->config.verbose) {
        printf("Cloud: Generating response in %s\n", language);
    }

    snprintf(prompt, sizeof(prompt), RESPONSE_GENERATION_PROMPT,
             language, user_input ? user_input : "", game_response);

    return nagi_llm_cloud_generate(llm, prompt, output, output_size);
}
