#ifndef LLM_UTILS_H
#define LLM_UTILS_H

#include <stdio.h>

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