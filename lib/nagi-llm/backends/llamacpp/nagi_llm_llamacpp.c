/*
 * llm_parser.c - LLM-based Input Parser for NAGI
 *
 * Implementation using llama.cpp for natural language understanding.
 *
 * NOTE: This file requires llama.cpp to be linked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#include "nagi_llm_llamacpp.h"
#include "../../include/nagi_llm_context.h"

#include "llama.h"

#include <stdint.h>
#include <stddef.h>

/* Basic types */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

/* Local endian conversion function (big-endian to host) */
static inline u16 load_be_16(const u8 *ptr) {
    return (u16)((ptr[0] << 8) | ptr[1]);
}

/* Prompt template for EXTRACTION mode - will be filled with game vocabulary */
static const char *EXTRACTION_PROMPT_TEMPLATE =
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: mira el castillo<|end|>\n"
    "<|assistant|>\n"
    "look castle<|end|>\n"
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: coge la llave<|end|>\n"
    "<|assistant|>\n"
    "get key<|end|>\n"
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: %s<|end|>\n"
    "<|assistant|>\n";

/* Fallback prompt when dictionary not available */
static const char *EXTRACTION_PROMPT_SIMPLE =
    "<|user|>\n"
    "Translate to English (verb noun only):\n"
    "mira el castillo<|end|>\n"
    "<|assistant|>\n"
    "look castle<|end|>\n"
    "<|user|>\n"
    "Translate to English (verb noun only):\n"
    "coge la llave<|end|>\n"
    "<|assistant|>\n"
    "get key<|end|>\n"
    "<|user|>\n"
    "Translate to English (verb noun only):\n"
    "%s<|end|>\n"
    "<|assistant|>\n";

/* Prompt for response generation - translates game response to user's language */
static const char *RESPONSE_GENERATION_PROMPT =
    "<|user|>\n"
    "You are a text adventure game narrator. Translate ONLY the game response to match "
    "the exact language the player used. Do NOT include any context information in your answer.\n\n"
    "Player said: %s\n"
    "Game responded: %s\n"
    "%s\n"  /* Optional context - for information only */
    "Translate ONLY the game response above to the player's language (Spanish, English, etc.). "
    "Output only the translated text, nothing else:<|end|>\n"
    "<|assistant|>\n";

/* Prompt for SEMANTIC mode - matches input meaning with expected command */
static const char *SEMANTIC_MATCHING_PROMPT =
    "<|system|>\n"
    "You are a command matcher for a text adventure game. Your job is to determine if a user's input "
    "(in any language) has the same meaning as a specific game command (in English).\n\n"
    "Rules:\n"
    "- If the input means the same action as the expected command, answer 'yes'\n"
    "- If the input means something different, answer 'no'\n"
    "- Only answer with 'yes' or 'no', nothing else\n"
    "<|end|>\n"
    "<|user|>\n"
    "Expected command: look castle\n"
    "User input: mira el castillo\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n"
    "yes<|end|>\n"
    "<|user|>\n"
    "Expected command: get key\n"
    "User input: coge la llave\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n"
    "yes<|end|>\n"
    "<|user|>\n"
    "Expected command: open door\n"
    "User input: abrir puerta\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n"
    "yes<|end|>\n"
    "<|user|>\n"
    "Expected command: quit\n"
    "User input: mira el castillo\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n"
    "no<|end|>\n"
    "<|user|>\n"
    "Expected command: fast\n"
    "User input: mira el castillo\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n"
    "no<|end|>\n"
    "<|user|>\n"
    "Expected command: restore game\n"
    "User input: mirar castillo\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n"
    "no<|end|>\n"
    "<|user|>\n"
    "Expected command: %s\n"
    "User input: %s\n"
    "Does the input match the command?<|end|>\n"
    "<|assistant|>\n";

/* Internal backend state */
typedef struct llm_state {
    /* For real LLM backend */
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;           /* For extraction/semantic (deterministic) */
    struct llama_sampler *sampler_creative;  /* For response generation (creative) */

    /* Common fields */
    int initialized;
    char last_error[256];

    /* Sequence counter for rotating through sequences */
    int seq_counter;

    /* Game dictionary (words.tok data) - passed from game engine */
    const u8 *dictionary_data;
    size_t dictionary_size;
} llm_state_t;


/* Common error setter */
static void set_error(llm_state_t *state, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (state) {
        vsnprintf(state->last_error, sizeof(state->last_error), fmt, args);
    }
    va_end(args);
}

static int llamacpp_init(nagi_llm_t *llm, const char *model_path, const nagi_llm_config_t *config)
{
    struct llama_model_params model_params;
    struct llama_context_params ctx_params;
    llm_state_t *state;

    /* Allocate state */
    state = (llm_state_t *)calloc(1, sizeof(llm_state_t));
    if (!state) {
        fprintf(stderr, "LLM Parser: Failed to allocate state\n");
        return 0;
    }
    
    /* Apply configuration */
    llm->impl_data = state;

    if (config) {
        memcpy(&llm->config, config, sizeof(nagi_llm_config_t));
    }
    if (model_path) {
        strncpy(llm->config.model_path, model_path, NAGI_LLM_MAX_MODEL_PATH - 1);
        llm->config.model_path[NAGI_LLM_MAX_MODEL_PATH - 1] = '\0';
    }

    /* Initialize llama backend */
    llama_backend_init();

    /* Load model */
    model_params = llama_model_default_params();
    if (llm->config.use_gpu) {
        model_params.n_gpu_layers = 999;
        model_params.main_gpu = 0;
    } else {
        model_params.n_gpu_layers = 0;
    }
    
    model_params.use_mmap = true;
    model_params.use_mlock = false;

    printf("LLM Parser: Loading model from %s...\n", llm->config.model_path);
    state->model = llama_model_load_from_file(llm->config.model_path, model_params);
    if (!state->model) {
        set_error(state, "Failed to load model: %s", llm->config.model_path);
        fprintf(stderr, "LLM Parser: %s\n", state->last_error);
        free(state);
        llm->impl_data = NULL;
        return 0;
    }

    /* Create context */
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = llm->config.context_size;
    ctx_params.n_batch = llm->config.batch_size;
    ctx_params.n_ubatch = llm->config.u_batch_size;
    ctx_params.n_threads = llm->config.n_threads;
    ctx_params.n_threads_batch = llm->config.n_threads;
    ctx_params.n_seq_max = 8;

    state->ctx = llama_init_from_model(state->model, ctx_params);
    if (!state->ctx) {
        set_error(state, "Failed to create context");
        fprintf(stderr, "LLM Parser: %s\n", state->last_error);
        llama_model_free(state->model);
        free(state);
        llm->impl_data = NULL;
        return 0;
    }

    /* Create sampler for extraction/semantic (deterministic) */
    state->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_k(llm->config.top_k));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_p(llm->config.top_p, 1));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_temp(llm->config.temperature));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_dist(42));

    /* Create sampler for response generation (creative) */
    state->sampler_creative = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_dist(42));

    /* Initialize sequence counter */
    state->seq_counter = 0;
    state->initialized = 1;
    
    printf("LLM Parser: Initialized successfully\n");
    printf("  Context size: %d\n", llm->config.context_size);
    printf("  Batch size: %d\n", llm->config.batch_size);
    printf("  Threads: %d\n", llm->config.n_threads);

    return 1;
}

/*
 * Shutdown the LLM parser
 */
static void llamacpp_shutdown(nagi_llm_t *llm)
{
    llm_state_t *state = (llm_state_t *)llm->impl_data;
    if (!state) return;

    if (state->sampler) llama_sampler_free(state->sampler);
    if (state->sampler_creative) llama_sampler_free(state->sampler_creative);
    if (state->ctx) llama_free(state->ctx);
    if (state->model) llama_model_free(state->model);
    llama_backend_free();
    
    free(state);
    llm->impl_data = NULL;
    printf("LLM Parser: Shutdown complete\n");
}

/*
 * Check if LLM is ready
 */
static int llamacpp_ready(nagi_llm_t *llm)
{
    llm_state_t *state = (llm_state_t *)llm->impl_data;
    return state && state->initialized;
}

/*
 * Helper: Get word string from word ID
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
static const char *get_word_string(nagi_llm_t *llm, int word_id)
{
    llm_state_t *state = (llm_state_t *)llm->impl_data;
    static char buffer[64];
    int i;
    int words_checked = 0;

    /* Verify that dictionary data is loaded */
    if (!state || !state->dictionary_data) {
        fprintf(stderr, "LLM Parser: dictionary not loaded\n");
        return NULL;
    }

    /* Iterate through all 26 letter offsets (A-Z) */
    for (i = 0; i < 26; i++) {
        /* Get offset for this letter from the header table */
        u16 offset = load_be_16(state->dictionary_data + i * 2);

        /* Skip if no words for this letter */
        if (offset == 0) continue;

        /* Start at the word list for this letter */
        u8 *ptr = state->dictionary_data + offset;

        /* Reset buffer for each letter section */
        buffer[0] = '\0';

        /* Iterate through all words starting with this letter */
        /* Each section ends with a 0 byte AFTER processing at least one word */
        int words_in_section = 0;
        while (1) {
            u8 prefix_count = *ptr;

            /* If we find 0 after processing at least one word, end of section */
            if (words_in_section > 0 && prefix_count == 0) {
                break;
            }

            ptr++;  /* Skip prefix count byte */
            words_checked++;
            words_in_section++;
            int len;

            /* Keep the prefix from previous word (compression technique) */
            /* Words like "look", "looks", "looking" share prefixes */
            len = prefix_count;

            /* Decode the remaining characters byte by byte */
            while (1) {
                u8 byte = *ptr;

                /* Decode character: remove bit 7 and XOR with 0x7F */
                char decoded_char = (byte & 0x7F) ^ 0x7F;

                /* Add to buffer if there's space */
                if (len < (int)sizeof(buffer) - 1) {
                    buffer[len++] = decoded_char;
                } else {
                    /* Word too long - this shouldn't happen with valid data */
                    fprintf(stderr, "LLM Parser: Word too long in words.tok\n");
                    break;
                }

                /* Check if this is the last character (bit 7 set) */
                if (byte & 0x80) {
                    ptr++;
                    break;
                }

                ptr++;
            }

            /* Null-terminate the decoded word */
            buffer[len] = '\0';

            /* Read the word ID (16-bit big-endian) */
            u16 id = load_be_16(ptr);

            /* Check if this is the word we're looking for */
            if (id == (u16)word_id) {
                if (llm->config.verbose) {
                    printf("LLM Parser: Found word_id %d -> \"%s\"\n", word_id, buffer);
                }
                return buffer;
            }

            /* Move to next word (skip the 2-byte ID) */
            ptr += 2;
        }
    }

    /* Word ID not found */
    if (llm->config.verbose) {
        fprintf(stderr, "LLM Parser: word_id %d not found in dictionary\n", word_id);
    }
    return NULL;
}

/*
 * Extract common verbs from the game dictionary
 * Returns a static buffer with comma-separated verb list (e.g., "look, get, open, close")
 * Extracts first 40-50 words which are typically verbs in AGI games
 */
static const char *extract_game_verbs(nagi_llm_t *llm)
{
    llm_state_t *state = (llm_state_t *)llm->impl_data;
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

/*
 * Extract verb and noun from user input (EXTRACTION mode)
 * Translates "mira el castillo" -> "look castle"
 * Much faster than semantic matching, uses shorter prompt
 * Returns static buffer with extracted English words
 */
static const char *llamacpp_extract_words(nagi_llm_t *llm, const char *input)
{
    static char response_buf[NAGI_LLM_MAX_RESPONSE_SIZE];
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    char piece[64];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_extract_tokens;
    llama_token *tokens;

    if (!llamacpp_ready(llm)) return input;
    if (!input || input[0] == '\0') return input;

    /* Extract game verbs for vocabulary hint */
    const char *verbs = extract_game_verbs(llm);

    /* Build extraction prompt with vocabulary context */
    if (verbs && verbs[0] != '\0') {
        /* Use template with verb vocabulary */
        snprintf(prompt, sizeof(prompt), EXTRACTION_PROMPT_TEMPLATE,
                 verbs, verbs, verbs, input);
    } else {
        /* Fallback to simple prompt */
        snprintf(prompt, sizeof(prompt), EXTRACTION_PROMPT_SIMPLE, input);
    }

    llm_state_t *state = (llm_state_t *)llm->impl_data;

    /* Use rotating sequence IDs to avoid KV cache conflicts */
    current_seq = (state->seq_counter++) % 8;

    if (llm->config.verbose) {
        printf("\n=== LLM Extraction ===\n");
        printf("Input: \"%s\"\n", input);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache for this sequence */
    llama_memory_t mem = llama_get_memory(state->ctx);
    llama_memory_seq_rm(mem, current_seq, -1, -1);

    /* Tokenize prompt */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return input;
    }

    /* Process prompt in batches */
    struct llama_batch batch = llama_batch_init(llm->config.batch_size, 0, 8);
    for (int i = 0; i < n_prompt_tokens; i += llm->config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > llm->config.batch_size) n_eval = llm->config.batch_size;

        batch.n_tokens = n_eval;
        for (int k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = current_seq;
            batch.logits[k] = false;
        }
        if (i + n_eval == n_prompt_tokens) {
            batch.logits[n_eval - 1] = true;
        }

        if (llama_decode(state->ctx, batch) != 0) {
            llama_batch_free(batch);
            free(tokens);
            return input;
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Generate response (extract English words) */
    response_len = 0;
    gen_count = 0;
    max_extract_tokens = 10;  // Short limit - we only need "verb noun"
    struct llama_batch batch_gen = llama_batch_init(1, 0, 8);

    while (response_len < (int)sizeof(response_buf) - 1 && gen_count < max_extract_tokens) {
        llama_token new_token = llama_sampler_sample(state->sampler, state->ctx, -1);
        llama_sampler_accept(state->sampler, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(state->model), new_token)) {
            break;
        }

        int piece_len = llama_token_to_piece(llama_model_get_vocab(state->model),
                                             new_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < (int)sizeof(response_buf) - 1) {
            memcpy(response_buf + response_len, piece, piece_len);
            response_len += piece_len;

            /* Stop if we hit a newline - extraction should be one line only */
            if (strchr(piece, '\n') != NULL) {
                break;
            }
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + gen_count;
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = current_seq;
        batch_gen.logits[0] = true;

        if (llama_decode(state->ctx, batch_gen) != 0) {
            break;
        }
        gen_count++;
    }

    llama_batch_free(batch_gen);
    response_buf[response_len] = '\0';

    /* Normalize: trim whitespace and lowercase */
    char *trimmed = response_buf;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') {
        trimmed++;
    }
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
        *end-- = '\0';
    }

    /* Convert to lowercase */
    for (int i = 0; trimmed[i]; i++) {
        trimmed[i] = tolower((unsigned char)trimmed[i]);
    }

    if (llm->config.verbose) {
        printf("Extracted: \"%s\"\n", trimmed);
        printf("===================\n\n");
    }

    /* Copy trimmed result back to start of buffer */
    if (trimmed != response_buf) {
        memmove(response_buf, trimmed, strlen(trimmed) + 1);
    }

    return response_buf;
}

/*
 * Helper: Check whether an input matches an expected AGI word list
 * Uses semantic matching: asks LLM "does input match command?"
 * Returns 1 if match, 0 otherwise
 */
static int llamacpp_matches_expected(nagi_llm_t *llm, const char *input,
                                     const int *expected_word_ids, int expected_count)
{    
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    char expected_command[256];
    char response_buf[256];
    char piece[64];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count;
    llama_token *tokens;

    if (!llamacpp_ready(llm)) return 0;
    if (expected_count == 0) return 0;

    /* Build expected command string from word IDs */
    expected_command[0] = '\0';
    for (int i = 0; i < expected_count; i++) {
        int word_id = expected_word_ids[i];

        const char *word_str = get_word_string(llm, word_id);

        if (word_str) {
            if (expected_command[0] != '\0') {
                strcat(expected_command, " ");
            }
            strcat(expected_command, word_str);
        }
    }

    if (expected_command[0] == '\0') return 0;

    /* Build semantic matching prompt */
    snprintf(prompt, sizeof(prompt),
        SEMANTIC_MATCHING_PROMPT,
        expected_command, input);

    llm_state_t *state = (llm_state_t *)llm->impl_data;

    /* Use rotating sequence IDs to avoid KV cache conflicts */
    current_seq = (state->seq_counter++) % 8;

    if (llm->config.verbose) {
        printf("\n=== LLM Matching ===\n");
        printf("User input: \"%s\"\n", input);
        printf("Expected: \"%s\"\n", expected_command);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache for this sequence before use */
    llama_memory_t mem = llama_get_memory(state->ctx);
    bool cleared = llama_memory_seq_rm(mem, current_seq, -1, -1);
    if (llm->config.verbose) {
        printf("KV cache clear for seq %d: %s\n", current_seq, cleared ? "SUCCESS" : "FAILED");
    }

    /* Tokenize */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return 0;
    }

    /* Process prompt */
    struct llama_batch batch = llama_batch_init(llm->config.batch_size, 0, 8);
    if (llm->config.verbose) {
        printf("Processing prompt: %d tokens\n", n_prompt_tokens);
    }
    for (int i = 0; i < n_prompt_tokens; i += llm->config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > llm->config.batch_size) n_eval = llm->config.batch_size;

        batch.n_tokens = n_eval;
        for (int k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = current_seq;
            batch.logits[k] = false;
        }
        if (i + n_eval == n_prompt_tokens) {
            batch.logits[n_eval - 1] = true;
        }
        if (llm->config.verbose) {
            printf("Decoding batch: tokens=%d, first_pos=%d, seq=%d\n", n_eval, i, current_seq);
        }
        if (llama_decode(state->ctx, batch) != 0) {
            if (llm->config.verbose) {
                printf("ERROR: llama_decode failed during prompt processing\n");
            }
            llama_batch_free(batch);
            free(tokens);
            return 0;
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Generate response */
    response_len = 0;
    gen_count = 0;
    struct llama_batch batch_gen = llama_batch_init(1, 0, 8);
    if (llm->config.verbose) {
        printf("Starting generation phase, prompt processed up to position %d\n", n_prompt_tokens - 1);
    }
    while (response_len < (int)sizeof(response_buf) - 1 && gen_count < llm->config.max_tokens) {
        llama_token new_token = llama_sampler_sample(state->sampler, state->ctx, -1);
        llama_sampler_accept(state->sampler, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(state->model), new_token)) {
            if (llm->config.verbose) {
                printf("Generation ended: EOG token after %d tokens\n", gen_count);
            }
            break;
        }

        int piece_len = llama_token_to_piece(llama_model_get_vocab(state->model),
                                             new_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < (int)sizeof(response_buf) - 1) {
            memcpy(response_buf + response_len, piece, piece_len);
            response_len += piece_len;
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + gen_count;
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = current_seq;
        batch_gen.logits[0] = true;

        if (llm->config.verbose && gen_count == 0) {
            printf("First generation decode: pos=%d, seq=%d\n", batch_gen.pos[0], current_seq);
        }

        if (llama_decode(state->ctx, batch_gen) != 0) {
            if (llm->config.verbose) {
                printf("ERROR: llama_decode failed during generation at token %d (pos=%d)\n",
                       gen_count, n_prompt_tokens + gen_count);
            }
            break;
        }
        gen_count++;
    }

    if (llm->config.verbose && gen_count >= llm->config.max_tokens) {
        printf("Generation stopped: max_tokens limit (%d) reached\n", llm->config.max_tokens);
    }

    llama_batch_free(batch_gen);
    response_buf[response_len] = '\0';

    /* Normalize response: convert to lowercase and trim whitespace */
    for (int i = 0; i < response_len; i++) {
        response_buf[i] = tolower((unsigned char)response_buf[i]);
    }

    /* Trim leading whitespace */
    char *trimmed = response_buf;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') {
        trimmed++;
    }

    if (llm->config.verbose) {
        printf("LLM response: \"%s\"\n", response_buf);
        printf("Trimmed response: \"%s\"\n", trimmed);
    }

    /* Check if response starts with "yes" or "no" (more strict matching) */
    if (strncmp(trimmed, "yes", 3) == 0) {
        if (llm->config.verbose) {
            printf("Result: MATCH\n===================\n\n");
        }
        return 1;
    }
    if (strncmp(trimmed, "no", 2) == 0) {
        if (llm->config.verbose) {
            printf("Result: NO MATCH\n===================\n\n");
        }
        return 0;
    }

    /* If LLM didn't give clear yes/no, assume no match (conservative) */
    if (llm->config.verbose) {
        printf("Result: NO MATCH (unclear response)\n===================\n\n");
    }
    return 0;
}

/*
 * Generate a game response using the LLM
 * Translates game response to player's language and optionally adds context
 */
static int llamacpp_generate_response(nagi_llm_t *llm, const char *game_response,
                                      const char *user_input, char *output, int output_size)
{
    llm_state_t *state = (llm_state_t *)llm->impl_data;

    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    char context_str[512];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_response_tokens;
    char piece[64];
    llama_token *tokens;

    if (!llamacpp_ready(llm)) return 0;
    if (!game_response || !user_input || !output || output_size <= 0) return 0;

    /* Get context if available */
    context_str[0] = '\0';
    #ifdef NAGI_ENABLE_LLM
    const char *context = llm_context_build();
    if (context && context[0] != '\0') {
        snprintf(context_str, sizeof(context_str), "Game context: %s\n", context);
    }
    #endif

    /* Build generation prompt */
    snprintf(prompt, sizeof(prompt), RESPONSE_GENERATION_PROMPT,
             user_input, game_response, context_str);

    /* Use rotating sequence IDs */
    current_seq = ((llm_state_t *)llm->impl_data)->seq_counter++ % 8;

    if (llm->config.verbose) {
        printf("\n=== LLM Response Generation ===\n");
        printf("User input: \"%s\"\n", user_input);
        printf("Game response: \"%s\"\n", game_response);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache for this sequence */
    llama_memory_t mem = llama_get_memory(((llm_state_t *)llm->impl_data)->ctx);
    llama_memory_seq_rm(mem, current_seq, -1, -1);

    /* Tokenize prompt */
    n_tokens = llama_n_ctx(((llm_state_t *)llm->impl_data)->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(((llm_state_t *)llm->impl_data)->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return 0;
    }

    /* Process prompt in batches */
    struct llama_batch batch = llama_batch_init(llm->config.batch_size, 0, 8);
    for (int i = 0; i < n_prompt_tokens; i += llm->config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > llm->config.batch_size) n_eval = llm->config.batch_size;

        batch.n_tokens = n_eval;
        for (int k = 0; k < n_eval; k++) {
            batch.token[k] = tokens[i + k];
            batch.pos[k] = i + k;
            batch.n_seq_id[k] = 1;
            batch.seq_id[k][0] = current_seq;
            batch.logits[k] = false;
        }
        if (i + n_eval == n_prompt_tokens) {
            batch.logits[n_eval - 1] = true;
        }

        if (llama_decode(((llm_state_t *)llm->impl_data)->ctx, batch) != 0) {
            llama_batch_free(batch);
            free(tokens);
            return 0;
        }
    }
    llama_batch_free(batch);
    free(tokens);

    /* Generate response using creative sampler */
    response_len = 0;
    gen_count = 0;
    max_response_tokens = 150;  /* Limit for adventure game responses */
    struct llama_batch batch_gen = llama_batch_init(1, 0, 8);

    while (response_len < output_size - 1 && gen_count < max_response_tokens) {
        llama_token new_token = llama_sampler_sample(((llm_state_t *)llm->impl_data)->sampler_creative, ((llm_state_t *)llm->impl_data)->ctx, -1);
        llama_sampler_accept(((llm_state_t *)llm->impl_data)->sampler_creative, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(((llm_state_t *)llm->impl_data)->model), new_token)) {
            break;
        }

        int piece_len = llama_token_to_piece(llama_model_get_vocab(((llm_state_t *)llm->impl_data)->model),
                                             new_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < output_size - 1) {
            memcpy(output + response_len, piece, piece_len);
            response_len += piece_len;
        }

        batch_gen.n_tokens = 1;
        batch_gen.token[0] = new_token;
        batch_gen.pos[0] = n_prompt_tokens + gen_count;
        batch_gen.n_seq_id[0] = 1;
        batch_gen.seq_id[0][0] = current_seq;
        batch_gen.logits[0] = true;

        if (llama_decode(((llm_state_t *)llm->impl_data)->ctx, batch_gen) != 0) {
            break;
        }
        gen_count++;
    }

    llama_batch_free(batch_gen);
    output[response_len] = '\0';

    /* Trim whitespace */
    char *trimmed = output;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') {
        trimmed++;
    }
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
        *end-- = '\0';
    }

    /* Move trimmed result to start of output buffer */
    if (trimmed != output) {
        memmove(output, trimmed, strlen(trimmed) + 1);
        response_len = strlen(output);
    }

    if (llm->config.verbose) {
        printf("Generated response: \"%s\"\n", output);
        printf("===============================\n\n");
    }

    return response_len;
}
    

/*
 * Wrapper function for set_dictionary vtable entry
 */

static int llamacpp_set_dictionary(nagi_llm_t *llm, const unsigned char *dict_data, size_t dict_size)
{
    llm_state_t *state = (llm_state_t *)llm->impl_data;
    if (!state) {
        fprintf(stderr, "LLM Parser: Cannot set dictionary - not initialized\n");
        return 0;
    }
    
    state->dictionary_data = dict_data;
    state->dictionary_size = dict_size;

    if (llm->config.verbose) {
        fprintf(stderr, "LLM Parser: Dictionary set (%zu bytes)\n", dict_size);
    }

    return 1;
}

/*
 * Create a llama.cpp backend instance
 */
nagi_llm_t *nagi_llm_llamacpp_create(void)
{
    nagi_llm_t *llm = (nagi_llm_t *)calloc(1, sizeof(nagi_llm_t));
    if (!llm) return NULL;

    llm->backend = NAGI_LLM_BACKEND_LLAMACPP;
    
    // Set default config
    llm->config.backend = NAGI_LLM_BACKEND_LLAMACPP;
    llm->config.context_size = NAGI_LLM_DEFAULT_CONTEXT_SIZE;
    llm->config.batch_size = NAGI_LLM_DEFAULT_BATCH_SIZE;
    llm->config.u_batch_size = NAGI_LLM_DEFAULT_U_BATCH_SIZE;
    llm->config.n_threads = NAGI_LLM_DEFAULT_THREADS;
    llm->config.temperature = 0.0f;
    llm->config.top_p = 0.9f;
    llm->config.top_k = 1;
    llm->config.max_tokens = 5;
    llm->config.use_gpu = 1;
    llm->config.verbose = 0;
    llm->config.mode = NAGI_LLM_MODE_EXTRACTION;
    
    // Assign vtable
    llm->init = llamacpp_init;
    llm->shutdown = llamacpp_shutdown;
    llm->ready = llamacpp_ready;
    llm->extract_words = llamacpp_extract_words;
    llm->matches_expected = llamacpp_matches_expected;
    llm->generate_response = llamacpp_generate_response;
    llm->set_dictionary = llamacpp_set_dictionary;
    llm->impl_data = NULL; 

    return llm;
}
