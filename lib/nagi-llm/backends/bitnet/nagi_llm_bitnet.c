/*
 * nagi_llm_bitnet.c - BitNet.cpp Backend Implementation (REAL)
 *
 * BitNet.cpp uses llama.cpp API with optimized 1.58-bit quantized kernels
 * Performance: 2-5x faster than standard llama.cpp, 4-5x less memory
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "nagi_llm_bitnet.h"
#include "../../include/nagi_llm_context.h"
#include "llama.h"
#include "llm_utils.h"

/*
 * Semantic matching - uses LLM to determine if input matches expected command
 * Same approach as llamacpp backend
 */
static int bitnet_matches_expected(nagi_llm_t *llm, const char *input,
                                   const int *expected_word_ids, int expected_count)
{
    llm_state_t *state = llm->state;
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

    /* Use rotating sequence IDs to avoid KV cache conflicts */
    current_seq = (state->seq_counter++) % 8;

    if (llm->config.verbose) {
        printf("\n=== BitNet Semantic Matching ===\n");
        printf("User input: \"%s\"\n", input);
        printf("Expected: \"%s\"\n", expected_command);
        printf("Using sequence ID: %d\n", current_seq);
    }

    /* Clear KV cache for this sequence before use */
    llama_kv_cache_seq_rm(state->ctx, current_seq, -1, -1);
    if (llm->config.verbose) {
        printf("KV cache cleared for seq %d\n", current_seq);
    }

    /* Tokenize */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(state->model,
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

        if (llama_token_is_eog(state->model, new_token)) {
            if (llm->config.verbose) {
                printf("Generation ended: EOG token after %d tokens\n", gen_count);
            }
            break;
        }

        int piece_len = llama_token_to_piece(state->model,
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

    /* Check if response starts with "yes" or "no" */
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
 * Generate response
 */
static int bitnet_generate_response(nagi_llm_t *llm, const char *game_response,
                                   const char *user_input, char *output, int output_size)
{
    llm_state_t *state = llm->state;
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_response_tokens;
    char piece[64];
    llama_token *tokens;

    if (!nagi_llm_ready(llm)) return 0;
    if (!game_response || !user_input || !output || output_size <= 0) return 0;

    /* Detect language if user provided input */
    const char *language = "English";
    if (user_input && user_input[0] != '\0') {
        language = llm_detect_language(llm, user_input);
    } else if (state->detected_language[0]) {
        language = state->detected_language;
    }

    /* Build prompt with explicit language */
    snprintf(prompt, sizeof(prompt), RESPONSE_GENERATION_PROMPT,
             language, game_response);

    /* Use rotating sequence IDs */
    current_seq = state->seq_counter++ % 8;

    if (llm->config.verbose) {
        printf("BitNet: Generating response in %s\n", language);
    }

    /* Clear KV cache for this sequence */
    llama_kv_cache_seq_rm(state->ctx, current_seq, -1, -1);

    /* Tokenize prompt */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    n_prompt_tokens = llama_tokenize(state->model,
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

        if (llama_token_is_eog(state->model, new_token)) {
            break;
        }

        int piece_len = llama_token_to_piece(state->model,
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

    if (llm->config.verbose && response_len > 0) {
        printf("Generated: \"%s\"\n", output);
    }

    return response_len;
}

/*
 * Create BitNet backend instance
 */
nagi_llm_t *nagi_llm_bitnet_create(void)
{
    nagi_llm_t *llm;

    llm = (nagi_llm_t *)calloc(1, sizeof(nagi_llm_t));
    if (!llm) {
        return NULL;
    }

    llm->backend = NAGI_LLM_BACKEND_BITNET;
    
    /* Set BitNet-specific prompt templates (Llama 3 format) */
    llm->extraction_prompt_template = EXTRACTION_PROMPT_TEMPLATE;
    llm->extraction_prompt_simple = EXTRACTION_PROMPT_SIMPLE;
    
    llm->matches_expected = bitnet_matches_expected;
    llm->generate_response = bitnet_generate_response;
    llm->state = NULL;

    /* Set default config for BitNet backend */
    llm->config.backend = NAGI_LLM_BACKEND_BITNET;
    llm->config.context_size = NAGI_LLM_DEFAULT_CONTEXT_SIZE;
    llm->config.batch_size = NAGI_LLM_DEFAULT_BATCH_SIZE;
    llm->config.u_batch_size = NAGI_LLM_DEFAULT_U_BATCH_SIZE;
    llm->config.n_threads = 6;
    llm->config.temperature = 0.0f;
    llm->config.top_p = 0.9f;
    llm->config.top_k = 1;
    llm->config.max_tokens = 5;
    llm->config.use_gpu = 0;
    llm->config.verbose = 0;
    llm->config.mode = NAGI_LLM_MODE_EXTRACTION;
    llm->config.flash_attn = false;  /* BitNet works best without flash attention */
    llm->config.n_seq_max = 1; //8;

    return llm;
}
