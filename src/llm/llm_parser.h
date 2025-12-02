/*
 * llm_parser.h - LLM-based Input Parser for NAGI
 *
 * This module provides natural language understanding using an embedded
 * LLM (llama.cpp). It can:
 * - Parse free-form player input into AGI verb/noun pairs
 * - Generate contextual responses
 * - Expand abbreviated or ambiguous commands
 * - Provide hints based on game context
 *
 * The LLM is configured with game-specific vocabulary and context
 * to accurately map natural language to AGI commands.
 */

#ifndef NAGI_LLM_PARSER_H
#define NAGI_LLM_PARSER_H

#include "../agi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LLM configuration */
#define LLM_MAX_MODEL_PATH 512
#define LLM_MAX_PROMPT_SIZE 4096
#define LLM_MAX_RESPONSE_SIZE 1024
#define LLM_DEFAULT_CONTEXT_SIZE 4096
#define LLM_DEFAULT_BATCH_SIZE 1024
#define LLM_DEFAULT_U_BATCH_SIZE 512
#define LLM_DEFAULT_THREADS 4

/*
 * LLM operation modes
 */
typedef enum {
    LLM_MODE_DISABLED = 0,   /* LLM disabled, use original parser only */
    LLM_MODE_EXTRACTION = 1, /* Extract verb+noun in English, use original said() matching (FAST) */
    LLM_MODE_SEMANTIC = 2    /* Semantic matching: compare input meaning with expected command (SLOW, PRECISE) */
} llm_mode_t;

/*
 * LLM configuration structure
 */
typedef struct {
    char model_path[LLM_MAX_MODEL_PATH];
    int context_size;
    int batch_size;
    int u_batch_size;
    int n_threads;
    float temperature;
    float top_p;
    int top_k;
    int max_tokens;
    int use_gpu;           /* 1 to use GPU acceleration */
    int verbose;           /* 1 for verbose output */
    llm_mode_t mode;       /* LLM operation mode */
} llm_config_t;

/*
 * LLM state (opaque, implementation in .c)
 */
typedef struct llm_state llm_state_t;

/* Global LLM state */
extern llm_state_t *g_llm_state;
extern llm_config_t g_llm_config;

/*
 * Initialize the LLM parser
 *
 * @param model_path: Path to GGUF model file
 * @param config: Configuration (NULL for defaults)
 * @return: 1 on success, 0 on failure
 */
int llm_parser_init(const char *model_path, const llm_config_t *config);

/*
 * Shutdown the LLM parser
 */
void llm_parser_shutdown(void);

/*
 * Check if LLM is initialized and ready
 */
int llm_parser_ready(void);

/*
 * Extract verb and noun from user input (any language) to English words
 * Returns a static buffer with extracted words (e.g., "look castle")
 * Used in EXTRACTION mode - faster than semantic matching
 */
const char *llm_parser_extract_words(const char *input);

/*
 * Check if input matches expected command (Semantic Match)
 * Used in SEMANTIC mode - slower but more precise
 */
int llm_parser_matches_expected(const char *input,
                                const int *expected_word_ids,
                                int expected_count);

/*
 * Generate a game response using the LLM
 * Translates game response to player's language and adds context
 *
 * @param game_response: Original game response (usually in English)
 * @param user_input: Original user input (in their language)
 * @param output: Output buffer
 * @param output_size: Size of output buffer
 * @return: Number of characters generated, or 0 on failure
 */
int llm_parser_generate_response(const char *game_response, const char *user_input,
                                  char *output, int output_size);

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_PARSER_H */
