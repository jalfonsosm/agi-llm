/*
 * nagi_llm_bitnet.c - BitNet.cpp Backend Implementation (REAL)
 *
 * BitNet.cpp uses llama.cpp API with optimized 1.58-bit quantized kernels
 * Performance: 2-5x faster than standard llama.cpp, 4-5x less memory
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nagi_llm_bitnet.h"
#include "../../include/nagi_llm_context.h"
#include "llama.h"
#include "llm_utils.h"

//TODO: proper format for bitnet?
/* Extraction prompt template */
static const char *EXTRACTION_PROMPT_TEMPLATE =
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: mira el castillo<|end|>\n"
    "<|assistant|>\n"
    "look castle<|end|>\n"
    "<|user|>\n"
    "Translate to English using these verbs: %s\n"
    "Input: %s<|end|>\n"
    "<|assistant|>\n";

static const char *EXTRACTION_PROMPT_SIMPLE =
    "<|user|>\n"
    "Translate to English (verb noun only):\n"
    "%s<|end|>\n"
    "<|assistant|>\n";

//TODO: static const char *RESPONSE_GENERATION_PROMPT =
// TODO: static const char *SEMANTIC_MATCHING_PROMPT =

/*
 * Extract words from input
 */
static const char *bitnet_extract_words(nagi_llm_t *llm, const char *input)
{
    static char response_buf[256];
    char prompt[NAGI_LLM_MAX_PROMPT_SIZE];
    char piece[64];
    int n_tokens, n_prompt_tokens;
    int current_seq;
    int response_len, gen_count, max_extract_tokens;
    llama_token *tokens;
    int i, k, n_eval;
    int piece_len;
    llm_state_t *state = llm->state;

    if (!nagi_llm_ready(llm)) return input;
    if (!input || input[0] == '\0') return input;

    /* Build prompt */
    const char *verbs = extract_game_verbs(state);
    if (verbs && verbs[0] != '\0') {
        snprintf(prompt, sizeof(prompt), EXTRACTION_PROMPT_TEMPLATE, verbs, verbs, input);
    } else {
        snprintf(prompt, sizeof(prompt), EXTRACTION_PROMPT_SIMPLE, input);
    }

    /* Use rotating sequences */
    current_seq = (state->seq_counter++) % 4;

    /* Clear KV cache */
    /* Clear KV cache */
    llama_kv_cache_seq_rm(state->ctx, current_seq, -1, -1);

    /* Tokenize */
    n_tokens = llama_n_ctx(state->ctx);
    tokens = (llama_token *)malloc(n_tokens * sizeof(llama_token));
    printf("BitNet: Tokenizing prompt: %s\n", prompt); fflush(stdout);
    n_prompt_tokens = llama_tokenize(state->model,
                                     prompt, (int)strlen(prompt),
                                     tokens, n_tokens, true, true);
    if (n_prompt_tokens < 0) {
        free(tokens);
        return input;
    }

    /* Process prompt */
    struct llama_batch batch = llama_batch_init(llm->config.batch_size, 0, 4);
    for (i = 0; i < n_prompt_tokens; i += llm->config.batch_size) {
        n_eval = n_prompt_tokens - i;
        if (n_eval > llm->config.batch_size) n_eval = llm->config.batch_size;

        batch.n_tokens = n_eval;
        for (k = 0; k < n_eval; k++) {
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

    /* Generate response */
    response_len = 0;
    gen_count = 0;
    max_extract_tokens = 10;

    struct llama_batch batch_gen = llama_batch_init(1, 0, 4);

    while (response_len < (int)sizeof(response_buf) - 1 && gen_count < max_extract_tokens) {
        llama_token new_token = llama_sampler_sample(state->sampler, state->ctx, -1);
        llama_sampler_accept(state->sampler, new_token);

        if (llama_token_is_eog(state->model, new_token)) {
            break;
        }

        piece_len = llama_token_to_piece(state->model,
                                        new_token, piece, sizeof(piece), 0, true);
        if (piece_len > 0 && response_len + piece_len < (int)sizeof(response_buf) - 1) {
            memcpy(response_buf + response_len, piece, piece_len);
            response_len += piece_len;

            if (strchr(piece, '\n') != NULL) break;
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

    /* Trim and lowercase */
    char *trimmed = response_buf;
    while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r' || *trimmed == '\t') {
        trimmed++;
    }
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
        *end-- = '\0';
    }

    for (i = 0; trimmed[i]; i++) {
        trimmed[i] = tolower((unsigned char)trimmed[i]);
    }

    if (trimmed != response_buf) {
        memmove(response_buf, trimmed, strlen(trimmed) + 1);
    }

    return response_buf;
}

/*
 * Semantic matching
 */
static int bitnet_matches_expected(nagi_llm_t *llm, const char *input,
                                   const int *expected_word_ids, int expected_count)
{
    llm_state_t *state = llm->state;
    const char *extracted;
    char *extracted_copy;
    char *token;
    int i, j;
    int match_count = 0;
    const u8 *dict_data;
    
    if (!nagi_llm_ready(llm) || !input || !expected_word_ids || expected_count <= 0) {
        return 0;
    }

    /* 1. Extract words from input (e.g., "look castle") */
    extracted = bitnet_extract_words(llm, input);
    if (!extracted || extracted[0] == '\0') {
        return 0;
    }

    /* 2. Check if extracted words match any expected words */
    /* We need to look up the string representation of expected_word_ids */
    if (!state->dictionary_data) {
        return 0;
    }
    
    dict_data = state->dictionary_data;

    /* Make a copy of extracted string to tokenize */
    extracted_copy = strdup(extracted);
    if (!extracted_copy) return 0;

    token = strtok(extracted_copy, " ,");
    while (token) {
        int token_matched = 0;
        
        /* Check this token against all expected words */
        for (i = 0; i < expected_count; i++) {
            int expected_id = expected_word_ids[i];
            
            /* Look up word string in dictionary */
            /* Dictionary format: 26 offsets (52 bytes) -> word nodes */
            /* We have to scan the whole dictionary to find the ID? That's slow. */
            /* AGI dictionary is optimized for string -> ID, not ID -> string. */
            /* However, we can optimize by caching or just scanning since dict is small (~20KB) */

            /* Scan dictionary for this ID */
            for (j = 0; j < 26; j++) {
                u16 offset = load_be_16(dict_data + j * 2);
                if (offset == 0 || offset >= state->dictionary_size) continue;

                const u8 *word_node = dict_data + offset;
                while (1) {
                    u16 id = load_be_16(word_node);
                    word_node += 2;
                    if (id == 0) break; /* End of list for this letter */

                    u8 len = *word_node++;
                    if (len == 0) break; /* Should not happen if id != 0 */
                    
                    /* Check if this is the word we are looking for */
                    if (id == expected_id) {
                        /* Compare with extracted token (check length first for safety) */
                        size_t token_len = strlen(token);
                        if (token_len == len &&
                            strncasecmp(token, (const char *)word_node, len) == 0) {
                            token_matched = 1;
                        }
                    }
                    
                    word_node += len;
                    if (token_matched) break;
                }
                if (token_matched) break;
            }
            if (token_matched) break;
        }
        
        if (token_matched) {
            match_count++;
        }
        
        token = strtok(NULL, " ,");
    }

    free(extracted_copy);

    /* If we matched at least one significant word, consider it a match */
    /* Adjust logic as needed - maybe we need to match ALL extracted words? */
    /* For now, if we found any of the expected words in the extraction, return true */
    return match_count > 0;
}

/*
 * Generate response (simplified for BitNet)
 */
 // TODO:
static int bitnet_generate_response(nagi_llm_t *llm, const char *game_response,
                                   const char *user_input, char *output, int output_size)
{
    llm_state_t *state = llm->state;

    if (!nagi_llm_ready(llm)) return 0;
    if (!game_response || !output || output_size <= 0) return 0;

    /* BitNet optimized for extraction, not generation - just return game response */
    strncpy(output, game_response, output_size - 1);
    output[output_size - 1] = '\0';

    return (int)strlen(output);
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
    llm->extract_words = bitnet_extract_words;
    llm->matches_expected = bitnet_matches_expected;
    llm->generate_response = bitnet_generate_response;
    llm->state = NULL;

    /* Set default config for BitNet backend */
    llm->config.backend = NAGI_LLM_BACKEND_BITNET;
    llm->config.context_size = 2048;  /* Smaller for BitNet efficiency */
    llm->config.batch_size = 512;
    llm->config.u_batch_size = NAGI_LLM_DEFAULT_U_BATCH_SIZE;
    llm->config.n_threads = 2;  /* BitNet optimized for fewer threads */
    llm->config.temperature = 0.0f;
    llm->config.top_p = 0.9f;
    llm->config.top_k = 1;
    llm->config.max_tokens = 5;
    llm->config.use_gpu = 0;  /* BitNet is CPU-optimized */
    llm->config.verbose = 1;
    llm->config.mode = NAGI_LLM_MODE_EXTRACTION;
    llm->config.flash_attn = false;  /* BitNet works best without flash attention */
    llm->config.n_seq_max = 1; /* TODO: is this the correct default? */

    return llm;
}
