#ifndef LLM_UTILS_H
#define LLM_UTILS_H

#include <stdio.h>

#ifndef START_OF_SYSTEM
#define START_OF_SYSTEM "<start_of_system>\\n"
#endif

#ifndef END_OF_SYSTEM
#define END_OF_SYSTEM "<end_of_system>\\n"
#endif

#ifndef START_OF_USER
#define START_OF_USER "<start_of_user>\\n"
#endif

#ifndef END_OF_USER
#define END_OF_USER "<end_of_user>\\n"
#endif

#ifndef START_OF_ASSISTANT
#define START_OF_ASSISTANT "<start_of_assistant>\\n"
#endif

#ifndef END_OF_ASSISTANT

#define END_OF_ASSISTANT "<end_of_assistant>\\n"
#endif

/* Prompt template for EXTRACTION mode - will be filled with game vocabulary */
static const char *EXTRACTION_PROMPT_TEMPLATE =
    START_OF_USER
    "Translate to English using these verbs: %s\n"
    "Input: mira el castillo" END_OF_USER
    START_OF_ASSISTANT
    "look castle" END_OF_ASSISTANT
    START_OF_USER
    "Translate to English using these verbs: %s\n"
    "Input: coge la llave" END_OF_USER
    START_OF_ASSISTANT
    "get key" END_OF_ASSISTANT
    START_OF_USER
    "Translate to English using these verbs: %s\n"
    "Input: %s" END_OF_USER
    START_OF_ASSISTANT;

/* Fallback prompt when dictionary not available */
static const char *EXTRACTION_PROMPT_SIMPLE =
    START_OF_USER
    "Translate to English (verb noun only):\n"
    "mira el castillo" END_OF_USER
    START_OF_ASSISTANT
    "look castle" END_OF_ASSISTANT
    START_OF_USER
    "Translate to English (verb noun only):\n"
    "coge la llave" END_OF_USER
    START_OF_ASSISTANT
    "get key" END_OF_ASSISTANT
    START_OF_USER
    "Translate to English (verb noun only):\n"
    "%s" END_OF_USER
    START_OF_ASSISTANT;

/* Prompt for response generation - translates game response to user's language */
static const char *RESPONSE_GENERATION_PROMPT =
    START_OF_USER
    "Translate the game response to the SAME language the player used.\n\n"
    "Player input: %s\n"
    "Game response (English): %s\n"
    "%s"
    "If player wrote in Spanish, respond in Spanish. If English, respond in English.\n"
    "Be creative, use humor and sarcasm. Output ONLY the translated response:" END_OF_USER
    START_OF_ASSISTANT;

/* Prompt for SEMANTIC mode - matches input meaning with expected command */
static const char *SEMANTIC_MATCHING_PROMPT =
    START_OF_SYSTEM
    "You are a command matcher for a text adventure game. Your job is to determine if a user's input "
    "(in any language) has the same meaning as a specific game command (in English).\n\n"
    "Rules:\n"
    "- If the input means the same action as the expected command, answer 'yes'\n"
    "- If the input means something different, answer 'no'\n"
    "- Only answer with 'yes' or 'no', nothing else\n"
    END_OF_SYSTEM
    START_OF_USER
    "Expected command: look castle\n"
    "User input: mira el castillo\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT
    "yes" END_OF_ASSISTANT
    START_OF_USER
    "Expected command: get key\n"
    "User input: coge la llave\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT
    "yes" END_OF_ASSISTANT
    START_OF_USER
    "Expected command: open door\n"
    "User input: abrir puerta\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT
    "yes" END_OF_ASSISTANT
    START_OF_USER
    "Expected command: quit\n"
    "User input: mira el castillo\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT
    "no" END_OF_ASSISTANT
    START_OF_USER
    "Expected command: fast\n"
    "User input: mira el castillo\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT
    "no" END_OF_ASSISTANT   
    START_OF_USER
    "Expected command: restore game\n"
    "User input: mirar castillo\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT
    "no" END_OF_ASSISTANT
    START_OF_USER
    "Expected command: %s\n"
    "User input: %s\n"
    "Does the input match the command?" END_OF_USER
    START_OF_ASSISTANT;

/* Forward declaration */
typedef struct nagi_llm nagi_llm_t;

/* Endian conversion function (big-endian to host) */
static inline u16 load_be_16(const u8 *ptr) {
    return (u16)((ptr[0] << 8) | ptr[1]);
}

/*
 * Get word string from word ID
 * Returns pointer to static buffer with decoded word, or NULL if not found
 */
const char *get_word_string(nagi_llm_t *llm, int word_id);

/*
 * Extract common verbs from the game dictionary
 * Returns a static buffer with comma-separated verb list (e.g., "look, get, open, close")
 * Extracts first 40-50 words which are typically verbs in AGI games
 */
const char *extract_game_verbs(nagi_llm_t *llm);

#endif /* LLM_UTILS_H */