/*
 * llm_utils.c - Shared utility functions for LLM backends
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "../include/nagi_llm.h"
#include "../include/llm_utils.h"
#include "../include/nagi_llm_context.h"

/* Endian conversion function (big-endian to host) */
static u16 load_be_16(const u8 *ptr) {
    return (u16)((ptr[0] << 8) | ptr[1]);
}

/*
 * Get word string from word ID
 *
 * Searches through the WORDS.TOK data structure to find the string
 * corresponding to a given word ID.
 *
 * WORDS.TOK format (Sierra AGI compression):
 * - First 52 bytes: 26 big-endian 16-bit offsets (one per letter A-Z)
 * - Each word starts with:
 *   1. Prefix count byte (how many chars shared with previous word)
 *   2. Encoded characters where:
 *      - Bits 0-6: character XOR 0x7F
 *      - Bit 7 (0x80): set if this is the last character
 *   3. 16-bit big-endian word ID
 * - Each letter section ends with a 0 byte
 *
 * Returns: pointer to static buffer with decoded word, or NULL if not found
 */
const char *get_word_string(nagi_llm_t *llm, int word_id)
{
    llm_state_t *state = llm->state;
    static char buffer[64];
    int i;

    if (!state || !state->dictionary_data) {
        return NULL;
    }

    /* Iterate through all 26 letter offsets (A-Z) */
    for (i = 0; i < 26; i++) {
        u16 offset = load_be_16(state->dictionary_data + i * 2);
        if (offset == 0) continue;

        u8 *ptr = state->dictionary_data + offset;
        buffer[0] = '\0';

        int words_in_section = 0;
        while (1) {
            u8 prefix_count = *ptr;
            if (words_in_section > 0 && prefix_count == 0) {
                break;
            }

            ptr++;
            words_in_section++;
            int len = prefix_count;

            /* Decode characters */
            while (1) {
                u8 byte = *ptr;
                char decoded_char = (byte & 0x7F) ^ 0x7F;

                if (len < (int)sizeof(buffer) - 1) {
                    buffer[len++] = decoded_char;
                } else {
                    /* Word too long - shouldn't happen with valid data */
                    if (llm->config.verbose) {
                        fprintf(stderr, "LLM: Word too long in dictionary\n");
                    }
                    break;
                }

                if (byte & 0x80) {
                    ptr++;
                    break;
                }
                ptr++;
            }

            buffer[len] = '\0';

            /* Read word ID */
            u16 id = load_be_16(ptr);

            if (id == (u16)word_id) {
                if (llm->config.verbose) {
                    printf("LLM: Found word_id %d -> \"%s\"\n", word_id, buffer);
                }
                return buffer;
            }

            ptr += 2;
        }
    }

    /* Word ID not found */
    if (llm->config.verbose) {
        fprintf(stderr, "LLM: word_id %d not found in dictionary\n", word_id);
    }
    return NULL;
}

/*
 * Extract common verbs from the game dictionary
 * Returns a static buffer with comma-separated verb list (e.g., "look, get, open, close")
 * Extracts first 40-50 words which are typically verbs in AGI games
 */
const char *extract_game_verbs(nagi_llm_t *llm)
{
    llm_state_t *state = llm->state;
    static char verb_list[512];
    static int verbs_extracted = 0;

    /* Only extract once */
    if (verbs_extracted) {
        return verb_list;
    }

    /* Verify that words.tok data is loaded */
    if (!state->dictionary_data) {
        fprintf(stderr, "LLM: dictionary_data not loaded, cannot extract verbs\n");
        return NULL;
    }

    verb_list[0] = '\0';
    int verb_count = 0;
    int max_verbs = 50;  /* First ~50 words are usually verbs in AGI */

    /* Iterate through all 26 letter offsets (A-Z) */
    for (int i = 0; i < 26 && verb_count < max_verbs; i++) {
        u16 offset = load_be_16(state->dictionary_data + i * 2);
        if (offset == 0) continue;

        u8 *ptr = state->dictionary_data + offset;
        char buffer[64];
        buffer[0] = '\0';

        int words_in_section = 0;
        while (verb_count < max_verbs) {
            u8 prefix_count = *ptr;

            /* End of section */
            if (words_in_section > 0 && prefix_count == 0) {
                break;
            }

            ptr++;
            words_in_section++;
            int len = prefix_count;

            /* Decode characters */
            while (1) {
                u8 byte = *ptr;
                char decoded_char = (byte & 0x7F) ^ 0x7F;

                if (len < (int)sizeof(buffer) - 1) {
                    buffer[len++] = decoded_char;
                }

                if (byte & 0x80) {
                    ptr++;
                    break;
                }
                ptr++;
            }

            buffer[len] = '\0';

            /* Read word ID and skip it */
            ptr += 2;

            /* Add to verb list if there's space */
            if (strlen(buffer) > 0) {
                if (verb_list[0] != '\0') {
                    strncat(verb_list, ", ", sizeof(verb_list) - strlen(verb_list) - 1);
                }
                strncat(verb_list, buffer, sizeof(verb_list) - strlen(verb_list) - 1);
                verb_count++;
            }
        }
    }

    verbs_extracted = 1;

    if (llm->config.verbose) {
        printf("LLM: Extracted %d verbs from dictionary: %s\n", verb_count, verb_list);
    }

    return verb_list;
}


