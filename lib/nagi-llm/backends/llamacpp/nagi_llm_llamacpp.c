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
#include <stdint.h>
#include <time.h>

#include "nagi_llm_llamacpp.h"
#include "../../include/nagi_llm_context.h"
#include "../../include/llm_utils.h"
#include "../llama_common.h"

#include "llama.h"

#include <stdint.h>
#include <stddef.h>

/* Forward declarations */
static int llamacpp_init(nagi_llm_t *llm, const char *model_path, const nagi_llm_config_t *config);
static void llamacpp_shutdown(nagi_llm_t *llm);
static const char *llamacpp_extract_words(nagi_llm_t *llm, const char *input);
static const char *llamacpp_detect_language(nagi_llm_t *llm, const char *input);

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

    if (!nagi_llm_ready(llm)) return 0;
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

    llm_state_t *state = llm->state;

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
 * Detect language from user input (wrapper for shared implementation)
 */
static const char *llamacpp_detect_language(nagi_llm_t *llm, const char *input)
{
    llm_state_t *state = llm->state;
    return llama_common_detect_language(llm, input, state->model, state->ctx, state->sampler);
}

/*
 * Generate a game response using the LLM
 * Translates game response to player's language and optionally adds context
 */
static int llamacpp_generate_response(nagi_llm_t *llm, const char *game_response,
                                      const char *user_input, char *output, int output_size)
{
    llm_state_t *state = llm->state;

    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    // char context_str[512];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_response_tokens;
    char piece[64];
    llama_token *tokens;

    if (!nagi_llm_ready(llm)) return 0;
    if (!game_response || !user_input || !output || output_size <= 0) return 0;

    /* Get context if available */
    /*
    context_str[0] = '\0';
    const char *context = llm_context_build();
    if (context && context[0] != '\0') {
        snprintf(context_str, sizeof(context_str), "Game context: %s\n", context);
    */

    /* Detect language if user provided input */
    const char *language = "English";
    if (user_input && user_input[0] != '\0') {
        language = llamacpp_detect_language(llm, user_input);
    } else if (state->detected_language[0]) {
        language = state->detected_language;
    }

    /* Build prompt with explicit language, including user input for context */
    snprintf(prompt, sizeof(prompt), RESPONSE_GENERATION_PROMPT,
             language, user_input ? user_input : "", game_response);

    if (llm->config.verbose) {
        printf("Generating response in %s\n", language);
    }

    /* Use rotating sequence IDs */
    current_seq = (state->seq_counter++) % 8;

    if (llm->config.verbose) {
        printf("\n=== LLM Response Generation ===\n");
        printf("User input: \"%s\"\n", user_input);
        printf("Game response: \"%s\"\n", game_response);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache completely for this sequence to prevent language contamination */
    llama_memory_t mem = llama_get_memory(state->ctx);
    llama_memory_seq_rm(mem, current_seq, -1, -1);

    /* Tokenize prompt (add_special=false to avoid double BOS) */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(llama_model_get_vocab(state->model),
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, false, true);
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

        if (llama_decode(state->ctx, batch) != 0) {
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
        llama_token new_token = llama_sampler_sample(state->sampler_creative, state->ctx, -1);
        llama_sampler_accept(state->sampler_creative, new_token);

        if (llama_vocab_is_eog(llama_model_get_vocab(state->model), new_token)) {
            break;
        }

        int piece_len = llama_token_to_piece(llama_model_get_vocab(state->model),
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

        if (llama_decode(state->ctx, batch_gen) != 0) {
            break;
        }
        gen_count++;
    }

    llama_batch_free(batch_gen);
    output[response_len] = '\0';

    /* Extract only the translation line (first line or after "Translate:") */
    char *translate_marker = strstr(output, "Translate:");
    char *start = translate_marker ? translate_marker + 10 : output;
    
    /* Skip whitespace */
    while (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t') {
        start++;
    }
    
    /* Find end of first line */
    char *end = start;
    while (*end && *end != '\n' && *end != '\r') {
        end++;
    }
    *end = '\0';
    
    /* Remove trailing whitespace and punctuation artifacts */
    end--;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '?' || *end == '!')) {
        if (*end == '?' || *end == '!') {
            /* Keep if it's the only punctuation at the end */
            char *check = end - 1;
            if (check > start && (*check == '?' || *check == '!')) {
                *end = '\0';
                end--;
            } else {
                break;
            }
        } else {
            *end = '\0';
            end--;
        }
    }
    
    /* Move to output buffer */
    if (start != output) {
        memmove(output, start, strlen(start) + 1);
    }
    response_len = (int)strlen(output);

    if (llm->config.verbose && response_len > 0) {
        printf("Generated: \"%s\"\n", output);
    }

    return response_len;
}

/* Common error setter */
static void set_error(llm_state_t *state, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (state) {
        vsnprintf(state->last_error, sizeof(state->last_error), fmt, args);
    } else {
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, args);
        fprintf(stderr, "LLM Error: %s\n", buf);
    }
    va_end(args);
}

/*
 * Initialize the llama.cpp backend
 */
static int llamacpp_init(nagi_llm_t *llm, const char *model_path, const nagi_llm_config_t *config)
{
    if (!llm) {
        return 0;
    }

    struct llama_model_params model_params;
    struct llama_context_params ctx_params;
    
    if (llm->state && llm->state->initialized) {
        fprintf(stderr, "LLM: Already initialized\n");
        return 1;
    }

    if (!llm->state) {
        llm->state = (llm_state_t *)calloc(1, sizeof(llm_state_t));
        if (!llm->state) {
            set_error(NULL, "Failed to allocate state");
            return 0;
        }
    }
    
    llm_state_t *state = llm->state;

    if (config) {
        memcpy(&llm->config, config, sizeof(nagi_llm_config_t));
    }

    if (model_path && model_path[0] != '\0') {
        strncpy(llm->config.model_path, model_path, NAGI_LLM_MAX_MODEL_PATH - 1);
        llm->config.model_path[NAGI_LLM_MAX_MODEL_PATH - 1] = '\0';
    }

    /* Initialize llama.cpp backend */
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
        state = NULL;
        llm->state = NULL;
        return 0;
    }

    /* Create context */
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = llm->config.context_size;
    ctx_params.n_batch = llm->config.batch_size;
    ctx_params.n_ubatch = llm->config.u_batch_size;
    ctx_params.n_threads = llm->config.n_threads;
    ctx_params.n_threads_batch = llm->config.n_threads;
    ctx_params.n_seq_max = llm->config.n_seq_max;

    state->ctx = llama_init_from_model(state->model, ctx_params);
    if (!state->ctx) {
        set_error(state, "Failed to create context");
        fprintf(stderr, "LLM Parser: %s\n", state->last_error);
        llama_model_free(state->model);
        free(state);
        llm->state = NULL;
        return 0;
    }

    /* Random seed for variety */
    uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)((uintptr_t)state);
    
    /* Create sampler for extraction/semantic (deterministic) */
    state->sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_k(llm->config.top_k));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_top_p(llm->config.top_p, 1));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_temp(llm->config.temperature));
    llama_sampler_chain_add(state->sampler, llama_sampler_init_dist(seed));

    /* Create sampler for response generation (creative with randomized temperature) */
    float creative_temp = 0.2f + ((float)(seed % 30) / 100.0f);  /* 0.2 to 0.5 */
    state->sampler_creative = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_top_p(0.9f, 1));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_temp(creative_temp));
    llama_sampler_chain_add(state->sampler_creative, llama_sampler_init_dist(seed + 1));
    
    if (llm->config.verbose) {
        printf("LLM Sampler: seed=%u, creative_temp=%.2f\n", seed, creative_temp);
    }

    state->initialized = 1;
    state->seq_counter = 0;

    if (llm->config.verbose) {
        printf("LLM Parser: Initialized successfully\n");
        printf("  Context size: %d\n", llm->config.context_size);
        printf("  Batch size: %d\n", llm->config.batch_size);
        printf("  Threads: %d\n", llm->config.n_threads);
        printf("  Sequences: %d (seq 0 reserved for system prompt)\n", llm->config.n_seq_max);
    }

    return 1;
}

/*
 * Shutdown the llama.cpp backend
 */
static void llamacpp_shutdown(nagi_llm_t *llm)
{
    llm_state_t *state = llm->state;

    if (!state) return;

    if (llm->config.verbose) {
        fprintf(stderr, "LLM: Shutting down\n");
    }

    if (state->sampler) {
        llama_sampler_free(state->sampler);
    }
    if (state->sampler_creative) {
        llama_sampler_free(state->sampler_creative);
    }
    if (state->ctx) {
        llama_free(state->ctx);
    }
    if (state->model) {
        llama_model_free(state->model);
    }

    llama_backend_free();

    free(state);
    llm->state = NULL;

    if (llm->config.verbose) {
        printf("LLM Parser: Shutdown complete\n");
    }
}

/*
 * Extract verb and noun from user input (EXTRACTION mode)
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

    if (!nagi_llm_ready(llm)) return input;
    if (!input || input[0] == '\0') return input;

    /* Extract game verbs for vocabulary hint */
    const char *verbs = extract_game_verbs(llm);

    /* Build extraction prompt with vocabulary context */
    if (verbs && verbs[0] != '\0' && llm->extraction_prompt_template) {
        snprintf(prompt, sizeof(prompt), llm->extraction_prompt_template,
                 verbs, verbs, verbs, input);
    } else if (llm->extraction_prompt_simple) {
        snprintf(prompt, sizeof(prompt), llm->extraction_prompt_simple, input);
    } else {
        return input;
    }

    llm_state_t *state = llm->state;
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
    max_extract_tokens = 10;
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
 * Create a llama.cpp backend instance
 */
nagi_llm_t *nagi_llm_llamacpp_create(void)
{
    nagi_llm_t *llm = (nagi_llm_t *)calloc(1, sizeof(nagi_llm_t));
    if (!llm) return NULL;

    llm->backend = NAGI_LLM_BACKEND_LLAMACPP;

    llm->extraction_prompt_template = EXTRACTION_PROMPT_TEMPLATE;
    llm->extraction_prompt_simple = EXTRACTION_PROMPT_SIMPLE;
    
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
    llm->config.n_seq_max = 8;
    llm->config.flash_attn = true;
    
    /* Assign function pointers */
    llm->init = llamacpp_init;
    llm->shutdown = llamacpp_shutdown;
    llm->extract_words = llamacpp_extract_words;
    llm->matches_expected = llamacpp_matches_expected;
    llm->generate_response = llamacpp_generate_response;
    llm->state = NULL; 
    llm->backend = NAGI_LLM_BACKEND_LLAMACPP;

    return llm;
}
