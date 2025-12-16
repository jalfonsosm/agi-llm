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

/* Basic types */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

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
    NAGI_LLM_BACKEND_UNDEFINED = -1,
    NAGI_LLM_BACKEND_LLAMACPP = 0,  /* llama.cpp - embedded local LLM */
    NAGI_LLM_BACKEND_BITNET = 1,    /* BitNet - optimized quantized models */
    NAGI_LLM_BACKEND_CLOUD = 2      /* Cloud API (OpenAI-compatible) */
} nagi_llm_backend_t;

/*
 * LLM configuration structure
 */
typedef struct {
    nagi_llm_backend_t backend;                 /* Backend type */
    char model_path[NAGI_LLM_MAX_MODEL_PATH];   /* Path to model file (for local backends) */
    char api_key[256];                          /* API key (for cloud backends) */
    char api_endpoint[512];                     /* API endpoint URL (for cloud backends) */
    int context_size;
    int batch_size;
    int u_batch_size;
    int n_threads;
    float temperature;                          /* Extraction temperature (always 0.0 for deterministic) */
    float temperature_creative_base;            /* Base creative temperature for response generation */
    float temperature_creative_offset;          /* Random offset for creative temperature variation */
    float top_p;
    int top_k;
    int max_tokens;
    int use_gpu;                                /* 1 to use GPU acceleration (for local backends) */
    int verbose;                                /* 1 for verbose output */
    nagi_llm_mode_t mode;                       /* LLM operation mode */
    int flash_attn;
    int n_seq_max;

} nagi_llm_config_t;

/* backend state */
typedef struct llm_state {
    struct llama_model *model;
    struct llama_context *ctx;
    struct llama_sampler *sampler;           /* For extraction/semantic (deterministic) */
    struct llama_sampler *sampler_creative;  /* For response generation (creative) */

    /* Common fields */
    int initialized;
    char last_error[256];

    /* Sequence counter for rotating through sequences (1-7, seq 0 is reserved for system prompt) */
    int seq_counter;

    /* Detected language cache */
    char detected_language[32];

    /* Game dictionary (words.tok data) - passed from game engine */
    const u8 *dictionary_data;
    size_t dictionary_size;
} llm_state_t;

/*
 * Forward declaration of abstract LLM interface
 */
typedef struct nagi_llm nagi_llm_t;

/*
 * Abstract LLM interface - function pointer table (vtable)
 */
struct nagi_llm {
    llm_state_t *state;

    /* Backend type */
    nagi_llm_backend_t backend;

    /* Configuration */
    nagi_llm_config_t config;
    
    /* Backend-specific data */
    void *backend_data;

    /* Backend-specific prompt templates */
    const char *extraction_prompt_template;
    const char *extraction_prompt_simple;

    /* Backend function pointers */
    
    /*
     * Initialize the backend
     */
    int (*init)(nagi_llm_t *llm, const char *model_path, const nagi_llm_config_t *config);
    
    /*
     * Shutdown the backend
     */
    void (*shutdown)(nagi_llm_t *llm);
    
    /*
     * Extract verb and noun from user input
     */
    const char *(*extract_words)(nagi_llm_t *llm, const char *input);

    /*
     * Check if input matches expected command (Semantic Match)
     *
     * @param llm: LLM instance
     * @param input: User input string
     * @param expected_word_ids: Array of expected word IDs
     * @param expected_count: Number of expected words
     * @return: 1 if matches, 0 otherwise
     */
    int (*matches_expected)(nagi_llm_t *llm, const char *input,
                           const int *expected_word_ids, int expected_count);

    /*
     * Generate a game response using the LLM
     *
     * @param llm: LLM instance
     * @param game_response: Original game response (usually in English)
     * @param user_input: Original user input (in their language)
     * @param output: Output buffer
     * @param output_size: Size of output buffer
     * @return: Number of characters generated, or 0 on failure
     */
    int (*generate_response)(nagi_llm_t *llm, const char *game_response,
                            const char *user_input, char *output, int output_size);
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
 * Initialize the LLM backend
 */
int nagi_llm_init(nagi_llm_t *llm, 
                const char *model_path,
                const nagi_llm_config_t *config);

/*
 * Shutdown the LLM backend
 */
void nagi_llm_shutdown(nagi_llm_t *llm);

/*
 * Check if LLM is ready
 */
int nagi_llm_ready(nagi_llm_t *llm);

/*
 * Set dictionary data
 */
int nagi_llm_set_dictionary(nagi_llm_t *llm, const unsigned char *dictionary, size_t size);

/*
 * Extract verb and noun from user input
 */
const char *nagi_llm_extract_words(nagi_llm_t *llm, const char *input);

/*
 * Check if input matches expected command
 */
int nagi_llm_matches_expected(nagi_llm_t *llm, const char *input,
                                    const int *expected_word_ids, int expected_count);

/*
 * Generate a game response
 */
int nagi_llm_generate_response(nagi_llm_t *llm, const char *game_response,
                                      const char *user_input, char *output, int output_size);

/*
 * Load unified configuration from llm_config.ini
 */
int nagi_llm_load_config(nagi_llm_config_t *config,
                         nagi_llm_backend_t backend,
                         const char *config_file);

/*
 * Utility functions
 */
void trim_whitespace(char *str);
int parse_config_line(const char *line, char *key, char *value, int max_len);
void set_error(llm_state_t *state, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_H */
