#ifndef LLM_UTILS_H
#define LLM_UTILS_H

/* endian conversion function (big-endian to host) */
static inline u16 load_be_16(const u8 *ptr) {
    return (u16)((ptr[0] << 8) | ptr[1]);
}

/*
 * Extract common verbs from the game dictionary
 * Returns a static buffer with comma-separated verb list (e.g., "look, get, open, close")
 * Extracts first 40-50 words which are typically verbs in AGI games
 */
static const char *extract_game_verbs(nagi_llm_t *llm)
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
        fprintf(stderr, "LLM Parser: state->dictionary_data not loaded, cannot extract verbs\n");
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
        printf("LLM Parser: Extracted %d verbs from dictionary: %s\n", verb_count, verb_list);
    }

    return verb_list;
}

// /* Extract game verbs from dictionary */
// static const char *extract_game_verbs_new(llm_state_t *state)
// {
//     static char verbs_buf[1024];
//     const u8 *dict_data;
//     u16 offsets[26];
//     int verb_count;
//     int i;
//     size_t current_len = 0;
//     char current_word[128];
//     int current_word_len = 0;

//     if (!state || !state->dictionary_data) {
//         return NULL;
//     }

//     dict_data = state->dictionary_data;

//     /* Read 26 letter offsets (52 bytes header) */
//     for (i = 0; i < 26; i++) {
//         offsets[i] = load_be_16(dict_data + i * 2);
//     }

//     /* Extract words (first ~30 words to give context) */
//     verbs_buf[0] = '\0';
//     verb_count = 0;
//     current_len = 0;

//     /* Iterate through letter blocks */
//     for (i = 0; i < 26 && verb_count < 30; i++) {
//         u16 offset = offsets[i];
//         if (offset == 0 || offset >= state->dictionary_size) continue;

//         const u8 *word_ptr = dict_data + offset;
        
//         /* Reset current word for new letter block? 
//            Actually AGI dictionary resets prefix for each letter block usually?
//            Let's assume yes or that prefix_len=0 for first word. */
//         current_word_len = 0;

//         /* Scan words in this block */
//         /* We'll scan a few words from each letter to get a mix */
//         int words_in_letter = 0;
        
//         while (words_in_letter < 5 && verb_count < 30) {
//             /* Check bounds */
//             if ((size_t)(word_ptr - dict_data) >= state->dictionary_size) break;

//             /* Read prefix length */
//             u8 prefix_len = *word_ptr++;
            
//             /* Safety check for prefix length */
//             if (prefix_len > current_word_len) prefix_len = current_word_len;
//             current_word_len = prefix_len;

//             /* Read characters */
//             while (1) {
//                 if ((size_t)(word_ptr - dict_data) >= state->dictionary_size) break;
                
//                 u8 c_enc = *word_ptr++;
//                 u8 c_dec = (c_enc & 0x7F) ^ 0x7F;
                
//                 if (current_word_len < sizeof(current_word) - 1) {
//                     current_word[current_word_len++] = (char)c_dec;
//                 }
                
//                 if (c_enc & 0x80) {
//                     /* End of word */
//                     break;
//                 }
//             }
//             current_word[current_word_len] = '\0';

//             /* Read Word ID */
//             if ((size_t)(word_ptr - dict_data) + 2 > state->dictionary_size) break;
//             u16 word_id = load_be_16(word_ptr);
//             word_ptr += 2;

//             /* Add to list if it's a valid word and not too short */
//             if (current_word_len > 1) {
//                 /* Sanitize just in case */
//                 int valid = 1;
//                 for (int k = 0; k < current_word_len; k++) {
//                     if (!isprint(current_word[k]) && current_word[k] != ' ') {
//                         valid = 0; 
//                         break;
//                     }
//                 }

//                 if (valid) {
//                     size_t needed = current_word_len + (verb_count > 0 ? 2 : 0) + 1;
//                     if (current_len + needed < sizeof(verbs_buf)) {
//                         if (verb_count > 0) {
//                             strcat(verbs_buf, ", ");
//                             current_len += 2;
//                         }
//                         strcat(verbs_buf, current_word);
//                         current_len += current_word_len;
//                         verb_count++;
//                     } else {
//                         /* Buffer full */
//                         return verbs_buf;
//                     }
//                 }
//             }
            
//             words_in_letter++;
            
//             /* Check if we reached end of block? 
//                AGI dictionary doesn't have explicit end of block, 
//                it just goes until next offset? 
//                Or maybe we just stop after N words. */
//         }
//     }

//     return verbs_buf[0] != '\0' ? verbs_buf : NULL;
// }

#endif /* LLM_UTILS_H */