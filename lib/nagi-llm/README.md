# NAGI-LLM: Abstract LLM Interface Library

NAGI-LLM is a modular, backend-agnostic library for integrating Large Language Models into the NAGI AGI game engine. It provides a unified interface for multiple LLM backends, allowing you to choose the best model for your needs.

## Features

- **Abstract Interface**: Single API works with all backends
- **Multiple Backends**: Support for local and cloud LLMs
- **Modular Design**: Easy to add new backends
- **Zero Dependencies**: Backends are completely independent
- **Type-Safe**: C interface with clear function signatures

## Supported Backends

### 1. llama.cpp Backend (IMPLEMENTED ✅)

High-performance local inference using llama.cpp.

**Features:**
- Metal GPU acceleration on macOS
- GGUF model support
- Streaming generation
- KV cache management

**Models Supported:**
- Phi-3-mini (default)
- Llama 3.x
- Mistral
- Any GGUF model

**Enable:**
```cmake
-DNAGI_LLM_ENABLE_LLAMACPP=ON
```

### 2. BitNet Backend (STUB ⏳)

Ultra-efficient 1.58-bit quantized models using BitNet.cpp.

**Features:**
- Extremely fast CPU inference
- Minimal memory usage
- No GPU required
- 1.58-bit quantization

**Models Supported:**
- BitNet models (when available)

**Enable:**
```cmake
-DNAGI_LLM_ENABLE_BITNET=ON
```

**Status:** Stub implementation - ready for BitNet.cpp integration

### 3. Cloud API Backend (PLANNED 📋)

Connect to powerful cloud LLMs via API.

**Features:**
- Claude (Anthropic)
- GPT-4 (OpenAI)
- Gemini (Google)
- Generic API support

**Enable:**
```cmake
-DNAGI_LLM_ENABLE_CLOUD_API=ON
```

**Status:** Planned for future release

## Usage Example

```c
#include "nagi_llm.h"

/* Create LLM instance */
nagi_llm_t *llm = nagi_llm_create(NAGI_LLM_BACKEND_LLAMACPP);

/* Configure */
nagi_llm_config_t config = {
    .backend = NAGI_LLM_BACKEND_LLAMACPP,
    .context_size = 4096,
    .temperature = 0.0f,
    .mode = NAGI_LLM_MODE_EXTRACTION
};

/* Initialize */
nagi_llm_init(llm, "path/to/model.gguf", &config);

/* Set game dictionary */
nagi_llm_set_dictionary(llm, dictionary_data, dictionary_size);

/* Use it */
const char *result = nagi_llm_extract_words(llm, "mira el castillo");
/* result: "look castle" */

/* Cleanup */
nagi_llm_destroy(llm);
```

## Switching Backends

Simply change the backend type when creating:

```c
/* Use llama.cpp */
nagi_llm_t *llm1 = nagi_llm_create(NAGI_LLM_BACKEND_LLAMACPP);

/* Use BitNet */
nagi_llm_t *llm2 = nagi_llm_create(NAGI_LLM_BACKEND_BITNET);

/* Use Cloud API */
nagi_llm_t *llm3 = nagi_llm_create(NAGI_LLM_BACKEND_CLOUD_API);
```

The rest of the code remains identical!

## LLM Operation Modes

### EXTRACTION Mode (Fast)
Extracts verb+noun from user input in any language to English.
```c
config.mode = NAGI_LLM_MODE_EXTRACTION;
```

### SEMANTIC Mode (Precise)
Checks if user input semantically matches expected command.
```c
config.mode = NAGI_LLM_MODE_SEMANTIC;
```

### DISABLED Mode
Falls back to original parser only.
```c
config.mode = NAGI_LLM_MODE_DISABLED;
```

## Building

### With llama.cpp only (default)
```bash
cmake -DNAGI_LLM_ENABLE_LLAMACPP=ON ..
```

### With BitNet
```bash
cmake -DNAGI_LLM_ENABLE_LLAMACPP=ON -DNAGI_LLM_ENABLE_BITNET=ON ..
```

### With all backends
```bash
cmake -DNAGI_LLM_ENABLE_LLAMACPP=ON \
      -DNAGI_LLM_ENABLE_BITNET=ON \
      -DNAGI_LLM_ENABLE_CLOUD_API=ON ..
```

## Architecture

```
lib/nagi-llm/
├── include/
│   ├── nagi_llm.h          # Public API
│   └── nagi_llm_context.h  # Context management
├── src/
│   ├── nagi_llm.c          # Factory and interface
│   └── nagi_llm_context.c  # Game context tracking
└── backends/
    ├── llamacpp/           # llama.cpp implementation
    │   ├── nagi_llm_llamacpp.h
    │   └── nagi_llm_llamacpp.c
    ├── bitnet/             # BitNet.cpp implementation
    │   ├── nagi_llm_bitnet.h
    │   └── nagi_llm_bitnet.c
    └── cloudapi/           # Cloud API implementation (future)
        ├── nagi_llm_cloudapi.h
        └── nagi_llm_cloudapi.c
```

## Adding a New Backend

1. Create backend directory: `backends/mybackend/`
2. Create header: `nagi_llm_mybackend.h`
3. Implement vtable functions in `nagi_llm_mybackend.c`:
   - `mybackend_init()`
   - `mybackend_shutdown()`
   - `mybackend_ready()`
   - `mybackend_extract_words()`
   - `mybackend_matches_expected()`
   - `mybackend_generate_response()`
   - `mybackend_set_dictionary()`
4. Create factory: `nagi_llm_mybackend_create()`
5. Register in `src/nagi_llm.c`
6. Add to `CMakeLists.txt`

Done! Your backend is now available through the unified API.

## License

Part of the NAGI project. See main repository for license information.

## Contributing

Contributions welcome! Especially:
- BitNet.cpp integration
- Cloud API implementation
- New backend types
- Performance improvements

See the main NAGI repository for contribution guidelines.
