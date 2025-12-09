/*
 * llama_common.h - Shared utilities for llama.cpp-based backends
 * 
 * This header provides common implementations for backends that use
 * llama.cpp API (llamacpp and BitNet backends).
 */

#ifndef LLAMA_COMMON_H
#define LLAMA_COMMON_H

#include "../include/nagi_llm.h"
#include "../include/llm_utils.h"
#include "llama.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* API abstraction macros */
#ifdef NAGI_LLM_HAS_BITNET
#define LLAMA_TOKENIZE(model, prompt, len, tokens, n_tokens) \
    llama_tokenize(model, prompt, len, tokens, n_tokens, false, true)
#define LLAMA_KV_CLEAR(ctx, seq, p0, p1) llama_kv_cache_seq_rm(ctx, seq, p0, p1)
#define LLAMA_IS_EOG(model, token) llama_token_is_eog(model, token)
#define LLAMA_TOKEN_TO_PIECE(model, token, buf, size) \
    llama_token_to_piece(model, token, buf, size, 0, true)
#else
#define LLAMA_TOKENIZE(model, prompt, len, tokens, n_tokens) \
    llama_tokenize(llama_model_get_vocab(model), prompt, len, tokens, n_tokens, false, true)
#define LLAMA_KV_CLEAR(ctx, seq, p0, p1) \
    do { llama_memory_t mem = llama_get_memory(ctx); llama_memory_seq_rm(mem, seq, p0, p1); } while(0)
#define LLAMA_IS_EOG(model, token) llama_vocab_is_eog(llama_model_get_vocab(model), token)
#define LLAMA_TOKEN_TO_PIECE(model, token, buf, size) \
    llama_token_to_piece(llama_model_get_vocab(model), token, buf, size, 0, true)
#endif

/*
 * Detect language from user input - shared implementation
 */
static inline const char *llama_common_detect_language(nagi_llm_t *llm, const char *input,
                                                       struct llama_model *model,
                                                       struct llama_context *ctx,
                                                       struct llama_sampler *sampler)
{
    llm_state_t *state = llm->state;
    char prompt[512];
    int n_tokens, n_prompt_tokens;
    llama_token *tokens;

    if (!nagi_llm_ready(llm)) return "English";
    if (!input || input[0] == '\0') {
        return state->detected_language[0] ? state->detected_language : "English";
    }

    snprintf(prompt, sizeof(prompt), LANGUAGE_DETECTION_PROMPT, input);

    n_tokens = llama_n_ctx(ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    
    /* Tokenize */
    n_prompt_tokens = LLAMA_TOKENIZE(model, prompt, (int)strlen(prompt), tokens, n_tokens);
    
    if (n_prompt_tokens < 0) {
        free(tokens);
        return "English";
    }

    /* Use dedicated sequence 7 for language detection */
    int lang_seq = 7;
    
    /* Clear KV cache */
    LLAMA_KV_CLEAR(ctx, lang_seq, -1, -1);

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

        if (llama_decode(ctx, batch) != 0) {
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
        llama_token lang_token = llama_sampler_sample(sampler, ctx, -1);
        llama_sampler_accept(sampler, lang_token);
        
        /* Check for end of generation */
        if (LLAMA_IS_EOG(model, lang_token)) break;
        
        char piece[16];
        /* Token to piece */
        int piece_len = LLAMA_TOKEN_TO_PIECE(model, lang_token, piece, sizeof(piece));
        
        if (piece_len > 0) {
            if (detected_len + piece_len < 60) {
                memcpy(detected + detected_len, piece, piece_len);
                detected_len += piece_len;
            }
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
        strcpy(state->detected_language, p);
    } else {
        strcpy(state->detected_language, "English");
    }

    if (llm->config.verbose) {
        printf("Language detected: '%s' from input: '%s'\n", 
               state->detected_language, input);
    }

    /* Clear language detection sequence */
    LLAMA_KV_CLEAR(ctx, lang_seq, -1, -1);

    return state->detected_language;
}

#endif /* LLAMA_COMMON_H */
