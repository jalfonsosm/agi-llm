/*
 * nagi_llm.h - Abstract LLM Parser Interface for NAGI
 *
 * This module provides an abstract interface for LLM-based natural language
 * understanding. It supports multiple backend implementations:
 * - llama.cpp (embedded local LLM)
 * - BitNet (optimized quantized models)
 * - Cloud APIs (Claude, OpenAI, etc.)
 *
 * The interface allows switching between different LLM backends at runtime.
 */

#ifndef NAGI_LLM_H
#define NAGI_LLM_H

#include <stddef.h>  /* for NULL */

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define NAGI_LLM_MAX_MODEL_PATH 512
#define NAGI_LLM_MAX_PROMPT_SIZE 4096
#define NAGI_LLM_MAX_RESPONSE_SIZE 1024
#define NAGI_LLM_DEFAULT_CONTEXT_SIZE 4096
#define NAGI_LLM_DEFAULT_BATCH_SIZE 1024
#define NAGI_LLM_DEFAULT_U_BATCH_SIZE 512
#define NAGI_LLM_DEFAULT_THREADS 4

/*
 * LLM operation modes
 */
typedef enum {
    NAGI_LLM_MODE_DISABLED = 0,   /* LLM disabled, use original parser only */
    NAGI_LLM_MODE_EXTRACTION = 1, /* Extract verb+noun in English, use original said() matching (FAST) */
    NAGI_LLM_MODE_SEMANTIC = 2    /* Semantic matching: compare input meaning with expected command (SLOW, PRECISE) */
} nagi_llm_mode_t;

/*
 * LLM backend types
 */
typedef enum {
    NAGI_LLM_BACKEND_LLAMACPP = 0,  /* llama.cpp - embedded local LLM */
    NAGI_LLM_BACKEND_BITNET = 1,    /* BitNet - optimized quantized models */
    NAGI_LLM_BACKEND_CLOUD_API = 2  /* Cloud API (Claude, OpenAI, etc.) */
} nagi_llm_backend_t;

/*
 * LLM configuration structure
 */
typedef struct {
    nagi_llm_backend_t backend;         /* Backend type */
    char model_path[NAGI_LLM_MAX_MODEL_PATH];  /* Path to model file (for local backends) */
    char api_key[256];                  /* API key (for cloud backends) */
    char api_endpoint[512];             /* API endpoint URL (for cloud backends) */
    int context_size;
    int batch_size;
    int u_batch_size;
    int n_threads;
    float temperature;
    float top_p;
    int top_k;
    int max_tokens;
    int use_gpu;                        /* 1 to use GPU acceleration (for local backends) */
    int verbose;                        /* 1 for verbose output */
    nagi_llm_mode_t mode;              /* LLM operation mode */
} nagi_llm_config_t;

/*
 * Forward declaration of abstract LLM interface
 */
typedef struct nagi_llm nagi_llm_t;

/*
 * Abstract LLM interface - function pointer table (vtable)
 */
struct nagi_llm {
    /* Backend-specific data */
    void *impl_data;

    /* Backend type */
    nagi_llm_backend_t backend;

    /* Configuration */
    nagi_llm_config_t config;

    /*
     * Initialize the LLM backend
     *
     * @param self: LLM instance
     * @param model_path: Path to model file or API configuration
     * @param config: Configuration (NULL for defaults)
     * @return: 1 on success, 0 on failure
     */
    int (*init)(nagi_llm_t *self, const char *model_path, const nagi_llm_config_t *config);

    /*
     * Shutdown the LLM backend
     *
     * @param self: LLM instance
     */
    void (*shutdown)(nagi_llm_t *self);

    /*
     * Check if LLM is initialized and ready
     *
     * @param self: LLM instance
     * @return: 1 if ready, 0 otherwise
     */
    int (*ready)(nagi_llm_t *self);

    /*
     * Extract verb and noun from user input (any language) to English words
     *
     * @param self: LLM instance
     * @param input: User input string
     * @return: Static buffer with extracted words (e.g., "look castle"), or NULL on failure
     */
    const char *(*extract_words)(nagi_llm_t *self, const char *input);

    /*
     * Check if input matches expected command (Semantic Match)
     *
     * @param self: LLM instance
     * @param input: User input string
     * @param expected_word_ids: Array of expected word IDs
     * @param expected_count: Number of expected words
     * @return: 1 if matches, 0 otherwise
     */
    int (*matches_expected)(nagi_llm_t *self, const char *input,
                           const int *expected_word_ids, int expected_count);

    /*
     * Generate a game response using the LLM
     *
     * @param self: LLM instance
     * @param game_response: Original game response (usually in English)
     * @param user_input: Original user input (in their language)
     * @param output: Output buffer
     * @param output_size: Size of output buffer
     * @return: Number of characters generated, or 0 on failure
     */
    int (*generate_response)(nagi_llm_t *self, const char *game_response,
                            const char *user_input, char *output, int output_size);

    /*
     * Set dictionary data for the LLM backend
     *
     * @param self: LLM instance
     * @param dictionary: Pointer to dictionary data (e.g., words.tok)
     * @param size: Size of dictionary data in bytes
     * @return: 1 on success, 0 on failure
     */
    int (*set_dictionary)(nagi_llm_t *self, const unsigned char *dictionary, size_t size);
};

/*
 * Create a new LLM instance for the specified backend
 *
 * @param backend: Backend type
 * @return: New LLM instance, or NULL on failure
 */
nagi_llm_t *nagi_llm_create(nagi_llm_backend_t backend);

/*
 * Destroy an LLM instance
 *
 * @param llm: LLM instance to destroy
 */
void nagi_llm_destroy(nagi_llm_t *llm);

/*
 * Convenience functions that delegate to the vtable
 */

static inline int nagi_llm_init(nagi_llm_t *llm, const char *model_path,
                                const nagi_llm_config_t *config) {
    return llm && llm->init ? llm->init(llm, model_path, config) : 0;
}

static inline void nagi_llm_shutdown(nagi_llm_t *llm) {
    if (llm && llm->shutdown) {
        llm->shutdown(llm);
    }
}

static inline int nagi_llm_ready(nagi_llm_t *llm) {
    return llm && llm->ready ? llm->ready(llm) : 0;
}

static inline const char *nagi_llm_extract_words(nagi_llm_t *llm, const char *input) {
    return llm && llm->extract_words ? llm->extract_words(llm, input) : NULL;
}

static inline int nagi_llm_matches_expected(nagi_llm_t *llm, const char *input,
                                            const int *expected_word_ids, int expected_count) {
    return llm && llm->matches_expected ? llm->matches_expected(llm, input, expected_word_ids, expected_count) : 0;
}

static inline int nagi_llm_generate_response(nagi_llm_t *llm, const char *game_response,
                                              const char *user_input, char *output, int output_size) {
    return llm && llm->generate_response ? llm->generate_response(llm, game_response, user_input, output, output_size) : 0;
}

static inline int nagi_llm_set_dictionary(nagi_llm_t *llm, const unsigned char *dictionary, size_t size) {
    return llm && llm->set_dictionary ? llm->set_dictionary(llm, dictionary, size) : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_H */
