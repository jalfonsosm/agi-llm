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
#define LLM_DEFAULT_CONTEXT_SIZE 2048
#define LLM_DEFAULT_BATCH_SIZE 512
#define LLM_DEFAULT_THREADS 4

/*
 * Parse result structure
 * Contains the verb/noun mapping from natural language input
 */
typedef struct {
    int success;           /* 1 if parsing succeeded */
    int verb_id;           /* AGI verb word ID */
    int noun_id;           /* AGI noun word ID */
    int second_noun_id;    /* Second noun if present (e.g., "use X with Y") */
    char verb_str[64];     /* Verb as string */
    char noun_str[64];     /* Noun as string */
    char canonical[128];   /* Canonical form (e.g., "look at door") */
    float confidence;      /* Confidence score 0.0-1.0 */
    char llm_response[LLM_MAX_RESPONSE_SIZE];  /* Raw LLM response */
} llm_parse_result_t;

/*
 * Vocabulary entry for mapping words to AGI IDs
 */
typedef struct {
    int word_id;           /* AGI word ID */
    char word[64];         /* Word string */
    char synonyms[256];    /* Comma-separated synonyms */
    int is_verb;           /* 1 if verb, 0 if noun */
} llm_vocab_entry_t;

/*
 * LLM configuration structure
 */
typedef struct {
    char model_path[LLM_MAX_MODEL_PATH];
    int context_size;
    int batch_size;
    int n_threads;
    float temperature;
    float top_p;
    int top_k;
    int max_tokens;
    int use_gpu;           /* 1 to use GPU acceleration */
    int verbose;           /* 1 for verbose output */
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
 * Parse player input using the LLM
 *
 * @param input: Player's natural language input
 * @param context: Game context (from llm_context_build())
 * @param result: Output parse result
 * @return: 1 on success, 0 on failure
 */
int llm_parser_parse(const char *input, const char *context, llm_parse_result_t *result);

/*
 * Generate a response/description using the LLM
 *
 * @param prompt: Prompt for generation
 * @param context: Game context
 * @param output: Output buffer
 * @param output_size: Size of output buffer
 * @return: Number of characters generated
 */
int llm_parser_generate(const char *prompt, const char *context,
                        char *output, int output_size);

/*
 * Load vocabulary from AGI words.tok file
 *
 * @param words_data: Pointer to words.tok data
 * @param data_size: Size of data
 * @return: Number of words loaded
 */
int llm_parser_load_vocab(const u8 *words_data, int data_size);

/*
 * Load vocabulary from a text file
 * Format: word_id|word|synonyms|is_verb
 *
 * @param filename: Path to vocabulary file
 * @return: Number of words loaded
 */
int llm_parser_load_vocab_file(const char *filename);

/*
 * Add a vocabulary entry manually
 *
 * @param word_id: AGI word ID
 * @param word: Word string
 * @param synonyms: Comma-separated synonyms (can be NULL)
 * @param is_verb: 1 if verb, 0 if noun
 */
void llm_parser_add_vocab(int word_id, const char *word, const char *synonyms, int is_verb);

/*
 * Find word ID by string (case-insensitive)
 *
 * @param word: Word to find
 * @param is_verb: Output: 1 if verb, 0 if noun
 * @return: Word ID or -1 if not found
 */
int llm_parser_find_word(const char *word, int *is_verb);

/*
 * Set the system prompt for the LLM
 *
 * @param prompt: System prompt text
 */
void llm_parser_set_system_prompt(const char *prompt);

/*
 * Set game-specific prompt additions
 *
 * @param game_info: Game-specific information for the prompt
 */
void llm_parser_set_game_info(const char *game_info);

/*
 * Configure LLM parameters at runtime
 *
 * @param param: Parameter name
 * @param value: Parameter value (as string)
 */
void llm_parser_set_param(const char *param, const char *value);

/*
 * Get the last error message
 */
const char *llm_parser_get_error(void);

/*
 * Debug: Print vocabulary
 */
void llm_parser_print_vocab(void);

/*
 * Debug: Print LLM stats
 */
void llm_parser_print_stats(void);

/*
 * Helper: Parse AGI "said" word list from LLM response
 * Converts a response like "look,door" to word IDs
 *
 * @param response: LLM response string
 * @param word_ids: Output array of word IDs
 * @param max_words: Maximum words to parse
 * @return: Number of words parsed
 */
int llm_parser_response_to_words(const char *response, int *word_ids, int max_words);

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_PARSER_H */
