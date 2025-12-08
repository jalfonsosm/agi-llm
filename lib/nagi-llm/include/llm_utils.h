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
    "Input: regarde l'arbre" END_OF_USER
    START_OF_ASSISTANT
    "look tree" END_OF_ASSISTANT
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
    "regarde l'arbre" END_OF_USER
    START_OF_ASSISTANT
    "look tree" END_OF_ASSISTANT
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
/* Prompt for response generation - translates game texts to the player's language */
static const char *RESPONSE_GENERATION_PROMPT =
    START_OF_SYSTEM
    "You are a witty narrator for a text adventure game. Translate game texts to the player's language with creativity, humor, sarcasm and even irreverence. \n\n"
    "### CRITICAL LANGUAGE RULES ###\n"
    "1. IF 'Player input' is NOT EMPTY: ALWAYS use the language of the 'Player input'.\n"
    "2. IF 'Player input' IS EMPTY (Automatic Event/System Command): You MUST use the language of the LAST 'Respond:' in the conversation history to maintain continuity.\n"
    "3. Take care with translation cache in cases where the player switches languages for similar sentences.\n\n"
    END_OF_SYSTEM
    
    START_OF_USER
    "Player input: look at castle\n"
    "Game response: You see a tall castle\n"
    "Respond:" 
    END_OF_USER
    START_OF_ASSISTANT
    "You see a ridiculously tall castle. Compensating for something?" 
    END_OF_ASSISTANT
    
    START_OF_USER
    "Player input: mira el castillo\n"
    "Game response: You see a tall castle\n"
    "Respond:" 
    END_OF_USER
    START_OF_ASSISTANT
    "Ves un castillo ridículamente alto. ¿Compensando algo?" 
    END_OF_ASSISTANT
    
    START_OF_USER
    "Player input: \n"
    "Game response: A strong gust of wind blows past you.\n"
    "Respond:" 
    END_OF_USER
    START_OF_ASSISTANT
    "Una fuerte ráfaga de viento pasa a tu lado. ¿En serio creíste que podrías caminar sin despeinarte?" 
    END_OF_ASSISTANT
    
    START_OF_USER
    "Player input: open door\n"
    "Game response: The door is locked\n"
    "Respond:" 
    END_OF_USER
    START_OF_ASSISTANT
    "The door is locked. Shocking, right?" 
    END_OF_ASSISTANT
    
    START_OF_USER
    "Player input: regarde le château\n"
    "Game response: You see a tall castle\n"
    "Respond:" 
    END_OF_USER
    START_OF_ASSISTANT
    "Tu vois un château ridiculement haut. Il compense quelque chose?" 
    END_OF_ASSISTANT
    
    START_OF_USER
    "Player input: %s\n"
    "Game response: %s\n"
    "--- CRITICAL: REMEMBER: Respond in the Player Input language, but if Player input is empty, use the language of the LAST 'Respond:' in the history. Don't start repeating the player input or game response. ---"
    "Respond:" 
    END_OF_USER
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