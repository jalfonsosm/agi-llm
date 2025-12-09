#include "nagi_llm_cloud.h"
#include "../../include/llm_utils.h"
#include "../../include/nagi_llm_context.h"
#include <stdlib.h>
#include <string.h>

static int cloud_matches_expected(nagi_llm_t *llm, const char *input,
                                   const int *expected_word_ids, int expected_count);
static int cloud_generate_response(nagi_llm_t *llm, const char *game_response,
                                    const char *user_input, char *output, int output_size);

nagi_llm_t *nagi_llm_cloud_create(void) {
    nagi_llm_t *llm = (nagi_llm_t *)calloc(1, sizeof(nagi_llm_t));
    if (!llm) return NULL;
    
    llm->backend = NAGI_LLM_BACKEND_CLOUD;
    llm->matches_expected = cloud_matches_expected;
    llm->generate_response = cloud_generate_response;
    llm->extraction_prompt_template = EXTRACTION_PROMPT_TEMPLATE;
    llm->extraction_prompt_simple = EXTRACTION_PROMPT_SIMPLE;
    
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
    llm_state_t *state = llm->state;
    
    /* Detect language if user provided input */
    const char *language = "English";
    if (user_input && user_input[0] != '\0') {
        language = llm_detect_language(llm, user_input);
    } else if (state && state->detected_language[0]) {
        language = state->detected_language;
    }

    if (llm->config.verbose) {
        printf("Cloud: Generating response in %s\n", language);
    }

    /* Build prompt with explicit language */
    snprintf(prompt, sizeof(prompt), RESPONSE_GENERATION_PROMPT,
             language, game_response);

    return nagi_llm_cloud_generate(llm, prompt, output, output_size);
}
