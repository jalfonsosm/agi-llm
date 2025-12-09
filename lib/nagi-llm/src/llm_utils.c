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
#include "llama.h"

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

/*
 * Detect language from user input
 * Returns language name stored in state cache
 */
const char *llm_detect_language(nagi_llm_t *llm, const char *input)
{
    llm_state_t *state = llm->state;
    char prompt[512];
    int n_tokens, n_prompt_tokens;
    llama_token *tokens;

    if (!nagi_llm_ready(llm)) return "English";
    if (!input || input[0] == '\0') return state->detected_language[0] ? state->detected_language : "English";

    snprintf(prompt, sizeof(prompt), LANGUAGE_DETECTION_PROMPT, input);

    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, false, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return "English";
    }

    /* Use dedicated sequence 7 for language detection */
    int lang_seq = 7;
    llama_memory_t mem = llama_get_memory(state->ctx);
    llama_memory_seq_rm(mem, lang_seq, -1, -1);

    struct llama_batch batch = llama_batch_init(llm->config.batch_size, 0, 1);
    for (int i = 0; i < n_prompt_tokens; i += llm->config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > llm->config.batch_size) n_eval = llm->config.batch_size;

        batch.n_tokens = n_eval;
        for (int k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = lang_seq;
            batch.logits[k] = (i + k == n_prompt_tokens - 1);
        }

        if (llama_decode(state->ctx, batch) != 0) {
            llama_batch_free(batch);
            free(tokens);
            return "English";
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Sample language name with greedy decoding */
    char detected[64] = "";
    int detected_len = 0;
    
    for (int i = 0; i < 15 && detected_len < 60; i++) {
        llama_token lang_token = llama_sampler_sample(state->sampler, state->ctx, -1);
        llama_sampler_accept(state->sampler, lang_token);
        
        if (llama_vocab_is_eog(llama_model_get_vocab(state->model), lang_token)) {
            break;
        }
        
        char piece[16];
        int piece_len = llama_token_to_piece(llama_model_get_vocab(state->model),
                                             lang_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0) {
            if (detected_len + piece_len < 60) {
                memcpy(detected + detected_len, piece, piece_len);
                detected_len += piece_len;
            }
            /* Stop at newline */
            if (strchr(piece, '\n')) break;
        }
    }
    detected[detected_len] = '\0';
    
    /* Trim and validate */
    char *p = detected;
    while (*p == ' ' || *p == '\n') p++;
    char *end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\n' || *end == '.')) *end-- = '\0';
    
    /* Validate it's a known language */
    if (strncmp(p, "English", 7) == 0) {
        strcpy(state->detected_language, "English");
    } else if (strncmp(p, "Spanish", 7) == 0) {
        strcpy(state->detected_language, "Spanish");
    } else if (strncmp(p, "French", 6) == 0) {
        strcpy(state->detected_language, "French");
    } else if (strncmp(p, "German", 6) == 0) {
        strcpy(state->detected_language, "German");
    } else if (strncmp(p, "Italian", 7) == 0) {
        strcpy(state->detected_language, "Italian");
    } else if (strncmp(p, "Portuguese", 10) == 0) {
        strcpy(state->detected_language, "Portuguese");
    } else if (strncmp(p, "Russian", 7) == 0) {
        strcpy(state->detected_language, "Russian");
    } else if (strncmp(p, "Japanese", 8) == 0) {
        strcpy(state->detected_language, "Japanese");
    } else if (strncmp(p, "Chinese", 7) == 0) {
        strcpy(state->detected_language, "Chinese");
    } else if (strlen(p) > 2 && strlen(p) < 32) {
        /* Accept other languages if reasonable length */
        strcpy(state->detected_language, p);
    } else {
        strcpy(state->detected_language, "English");
    }

    if (llm->config.verbose) {
        printf("Language detected: '%s' from input: '%s'\n", 
               state->detected_language, input);
    }

    /* Clear language detection sequence */
    llama_memory_seq_rm(mem, lang_seq, -1, -1);

    return state->detected_language;
}
