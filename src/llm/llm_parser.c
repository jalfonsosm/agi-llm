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

#include "llm_parser.h"
#include "llm_context.h"

#include "llama.h"

#include "../agi.h"
#include "../ui/parse.h"
#include "../sys/endian.h"

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

struct llm_state {
    /* For real LLM backend */
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;

    /* Common fields */
    int initialized;
    char last_error[256];

    /* KV Cache Management */
    llama_token *tokens_prev;
    int n_tokens_prev;

    /* Sequence counter for rotating through sequences */
    int seq_counter;
};

llm_state_t *g_llm_state = NULL;
llm_config_t g_llm_config = {
    .model_path = "",
    .context_size = LLM_DEFAULT_CONTEXT_SIZE,
    .batch_size = LLM_DEFAULT_BATCH_SIZE,
    .u_batch_size = LLM_DEFAULT_U_BATCH_SIZE,
    .n_threads = LLM_DEFAULT_THREADS,
    .temperature = 0.0f,  // Greedy decoding for deterministic matching
    .top_p = 0.9f,
    .top_k = 1,   // Only consider top token
    .max_tokens = 5,  // Only need "yes" or "no"
    .use_gpu = 1,
    .verbose = 1 //0
};

/* Common error setter */
static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (g_llm_state) {
        vsnprintf(g_llm_state->last_error, sizeof(g_llm_state->last_error), fmt, args);
    } else {
        /* Fallback: print to stderr */
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        fprintf(stderr, "LLM Parser Error: %s\n", buf);
    }
    va_end(args);
}

int llm_parser_init(const char *model_path, const llm_config_t *config)
{
    struct llama_model_params model_params;
    struct llama_context_params ctx_params;

    if (g_llm_state && g_llm_state->initialized) {
        fprintf(stderr, "LLM Parser: Already initialized\n");
        return 1;
    }

    /* Allocate state */
    g_llm_state = (llm_state_t *)calloc(1, sizeof(struct llm_state));
    if (!g_llm_state) {
        fprintf(stderr, "LLM Parser: Failed to allocate state\n");
        return 0;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&g_llm_config, config, sizeof(llm_config_t));
    }
    if (model_path) {
        strncpy(g_llm_config.model_path, model_path, LLM_MAX_MODEL_PATH - 1);
    }

    /* Initialize llama backend */
    llama_backend_init();

    /* Load model */
    model_params = llama_model_default_params();
    model_params.n_gpu_layers = g_llm_config.use_gpu ? 99 : 0;

    printf("LLM Parser: Loading model from %s...\n", g_llm_config.model_path);
    g_llm_state->model = llama_model_load_from_file(g_llm_config.model_path, model_params);
    if (!g_llm_state->model) {
        set_error("Failed to load model: %s", g_llm_config.model_path);
        fprintf(stderr, "LLM Parser: %s\n", g_llm_state->last_error);
        free(g_llm_state);
        g_llm_state = NULL;
        return 0;
    }

    /* Create context */
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = g_llm_config.context_size;
    ctx_params.n_batch = g_llm_config.batch_size;
    ctx_params.n_ubatch = g_llm_config.u_batch_size;
    ctx_params.n_threads = g_llm_config.n_threads;
    ctx_params.n_threads_batch = g_llm_config.n_threads;
    ctx_params.n_seq_max = 8;

    g_llm_state->ctx = llama_init_from_model(g_llm_state->model, ctx_params);
    if (!g_llm_state->ctx) {
        set_error("Failed to create context");
        fprintf(stderr, "LLM Parser: %s\n", g_llm_state->last_error);
        llama_model_free(g_llm_state->model);
        free(g_llm_state);
        g_llm_state = NULL;
        return 0;
    }

    /* Create sampler */
    g_llm_state->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_top_k(g_llm_config.top_k));
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_top_p(g_llm_config.top_p, 1));
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_temp(g_llm_config.temperature));
    llama_sampler_chain_add(g_llm_state->sampler,
        llama_sampler_init_dist(42));

    /* Allocate KV cache tracking buffer */
    g_llm_state->tokens_prev = (llama_token *)malloc(g_llm_config.context_size * sizeof(llama_token));
    g_llm_state->n_tokens_prev = 0;

    /* Initialize sequence counter */
    g_llm_state->seq_counter = 0;

    g_llm_state->initialized = 1;
    printf("LLM Parser: Initialized successfully\n");
    printf("  Context size: %d\n", g_llm_config.context_size);
    printf("  Batch size: %d\n", g_llm_config.batch_size);
    printf("  Threads: %d\n", g_llm_config.n_threads);

    return 1;
}

/*
 * Shutdown the LLM parser
 */
void llm_parser_shutdown(void)
{
    if (!g_llm_state) return;

    if (g_llm_state->sampler) {
        llama_sampler_free(g_llm_state->sampler);
    }
    if (g_llm_state->ctx) {
        llama_free(g_llm_state->ctx);
    }
    if (g_llm_state->model) {
        llama_model_free(g_llm_state->model);
    }

    llama_backend_free();

    free(g_llm_state->tokens_prev);
    free(g_llm_state);
    g_llm_state = NULL;

    printf("LLM Parser: Shutdown complete\n");
}

/*
 * Check if LLM is ready
 */
int llm_parser_ready(void)
{
    return g_llm_state && g_llm_state->initialized;
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
static const char *get_word_string(int word_id)
{
    static char buffer[64];
    int i;
    int words_checked = 0;

    /* Verify that words.tok data is loaded */
    if (!words_tok_data) {
        fprintf(stderr, "LLM Parser: words_tok_data not loaded\n");
        return NULL;
    }

    /* Iterate through all 26 letter offsets (A-Z) */
    for (i = 0; i < 26; i++) {
        /* Get offset for this letter from the header table */
        u16 offset = load_be_16(words_tok_data + i * 2);

        /* Skip if no words for this letter */
        if (offset == 0) continue;

        /* Start at the word list for this letter */
        u8 *ptr = words_tok_data + offset;

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
                if (g_llm_config.verbose) {
                    printf("LLM Parser: Found word_id %d -> \"%s\"\n", word_id, buffer);
                }
                return buffer;
            }

            /* Move to next word (skip the 2-byte ID) */
            ptr += 2;
        }
    }

    /* Word ID not found */
    if (g_llm_config.verbose) {
        fprintf(stderr, "LLM Parser: word_id %d not found in dictionary\n", word_id);
    }
    return NULL;
}

/*
 * Helper: Check whether an input matches an expected AGI word list
 * Uses semantic matching: asks LLM "does input match command?"
 * Returns 1 if match, 0 otherwise
 */
int llm_parser_matches_expected(const char *input, const char *context,
                                const int *expected_word_ids, int expected_count,
                                float min_confidence)
{
    char prompt[LLM_MAX_PROMPT_SIZE];
    char expected_command[256];
    char response_buf[256];
    int n_tokens, n_prompt_tokens;
    llama_token *tokens;

    if (!llm_parser_ready()) return 0;
    if (expected_count == 0) return 0;

    /* Build expected command string from word IDs */
    expected_command[0] = '\0';
    for (int i = 0; i < expected_count; i++) {
        int word_id = expected_word_ids[i];

        const char *word_str = get_word_string(word_id);

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

    /* Use rotating sequence IDs to avoid KV cache conflicts */
    int current_seq = (g_llm_state->seq_counter++) % 8;

    if (g_llm_config.verbose) {
        printf("\n=== LLM Matching ===\n");
        printf("User input: \"%s\"\n", input);
        printf("Expected: \"%s\"\n", expected_command);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache for this sequence before use */
    llama_memory_t mem = llama_get_memory(g_llm_state->ctx);
    bool cleared = llama_memory_seq_rm(mem, current_seq, -1, -1);
    if (g_llm_config.verbose) {
        printf("KV cache clear for seq %d: %s\n", current_seq, cleared ? "SUCCESS" : "FAILED");
    }

    /* Tokenize */
    n_tokens = llama_n_ctx(g_llm_state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(g_llm_state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return 0;
    }

    /* Process prompt */
    struct llama_batch batch = llama_batch_init(g_llm_config.batch_size, 0, 8);
    if (g_llm_config.verbose) {
        printf("Processing prompt: %d tokens\n", n_prompt_tokens);
    }
    for (int i = 0; i < n_prompt_tokens; i += g_llm_config.batch_size) {
        int n_eval = n_prompt_tokens - i;
        if (n_eval > g_llm_config.batch_size) n_eval = g_llm_config.batch_size;

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
        if (g_llm_config.verbose) {
            printf("Decoding batch: tokens=%d, first_pos=%d, seq=%d\n", n_eval, i, current_seq);
        }
        if (llama_decode(g_llm_state->ctx, batch) != 0) {
            if (g_llm_config.verbose) {
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
    int response_len = 0;
    struct llama_batch batch_gen = llama_batch_init(1, 0, 8);
    if (g_llm_config.verbose) {
        printf("Starting generation phase, prompt processed up to position %d\n", n_prompt_tokens - 1);
    }
    int gen_count = 0;
    while (response_len < (int)sizeof(response_buf) - 1 && gen_count < g_llm_config.max_tokens) {
        llama_token new_token = llama_sampler_sample(g_llm_state->sampler, g_llm_state->ctx, -1);
        llama_sampler_accept(g_llm_state->sampler, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(g_llm_state->model), new_token)) {
            if (g_llm_config.verbose) {
                printf("Generation ended: EOG token after %d tokens\n", gen_count);
            }
            break;
        }

        char piece[64];
        int piece_len = llama_token_to_piece(llama_model_get_vocab(g_llm_state->model),
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

        if (g_llm_config.verbose && gen_count == 0) {
            printf("First generation decode: pos=%d, seq=%d\n", batch_gen.pos[0], current_seq);
        }

        if (llama_decode(g_llm_state->ctx, batch_gen) != 0) {
            if (g_llm_config.verbose) {
                printf("ERROR: llama_decode failed during generation at token %d (pos=%d)\n",
                       gen_count, n_prompt_tokens + gen_count);
            }
            break;
        }
        gen_count++;
    }

    if (g_llm_config.verbose && gen_count >= g_llm_config.max_tokens) {
        printf("Generation stopped: max_tokens limit (%d) reached\n", g_llm_config.max_tokens);
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

    if (g_llm_config.verbose) {
        printf("LLM response: \"%s\"\n", response_buf);
        printf("Trimmed response: \"%s\"\n", trimmed);
    }

    /* Check if response starts with "yes" or "no" (more strict matching) */
    if (strncmp(trimmed, "yes", 3) == 0) {
        if (g_llm_config.verbose) {
            printf("Result: MATCH\n===================\n\n");
        }
        return 1;
    }
    if (strncmp(trimmed, "no", 2) == 0) {
        if (g_llm_config.verbose) {
            printf("Result: NO MATCH\n===================\n\n");
        }
        return 0;
    }

    /* If LLM didn't give clear yes/no, assume no match (conservative) */
    if (g_llm_config.verbose) {
        printf("Result: NO MATCH (unclear response)\n===================\n\n");
    }
    return 0;
}
