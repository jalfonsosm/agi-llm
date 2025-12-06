# NAGI-LLM: Abstract LLM Interface Library

NAGI-LLM is a modular, backend-agnostic library for integrating Large Language Models into the NAGI AGI game engine. It provides a unified interface for multiple LLM backends, enabling natural language understanding in classic adventure games.

## Features

- **Abstract Interface**: Single API works with all backends
- **Multiple Backends**: Support for local and cloud LLMs  
- **Modular Design**: Easy to add new backends
- **Backend-Specific Optimizations**: Each backend uses optimal prompt formats and APIs
- **Shared Utilities**: Common dictionary parsing and verb extraction
- **Type-Safe**: Clean C interface with clear function signatures

## Supported Backends

| Backend | Status | Speed | Memory | GPU | Offline | Quality |
|---------|--------|-------|--------|-----|---------|---------|
| **llama.cpp** | ✅ Production | Fast | ~4GB | Optional | ✅ Yes | High |
| **BitNet** | ✅ Production | Ultra Fast | ~1.1GB | No (CPU-only) | ✅ Yes | Good |
| **Cloud API** | 📋 Planned | Network-dependent | Minimal | N/A | ❌ No | Excellent |

### 1. llama.cpp Backend ✅

High-performance local inference using llama.cpp with full GPU acceleration.

**Features:**
- Metal GPU acceleration on macOS
- GGUF model support
- Streaming generation
- Advanced KV cache management
- Phi-3 chat template format

**Default Model:** Phi-3-mini-4k-instruct-Q4_K_M.gguf (2.3GB)

**Enable:** `-DNAGI_LLM_ENABLE_LLAMACPP=ON`

### 2. BitNet Backend ✅

Ultra-efficient 1.58-bit quantized models using Microsoft's BitNet.cpp.

**Features:**
- Extremely fast CPU inference (2-5x faster than llama.cpp)
- Minimal memory usage (4-5x less than standard models)
- No GPU required
- 1.58-bit ternary quantization
- Llama 3 chat template format
- Optimized for low-end hardware

**Default Model:** BitNet-b1.58-2B-4T (1.1GB, 3.91 BPW)

**Performance:**
- Inference: ~10-20ms per token (CPU-only)
- Memory: ~1.1GB model + ~150MB KV cache
- Quality: 80-90% of full precision models

**Enable:** `-DNAGI_LLM_ENABLE_BITNET=ON`

### 3. Cloud API Backend 📋

**Status:** Planned for future release

## Quick Start

```c
#include "nagi_llm.h"

// Create LLM instance
nagi_llm_t *llm = nagi_llm_create(NAGI_LLM_BACKEND_BITNET);

// Configure
nagi_llm_config_t config = {
    .backend = NAGI_LLM_BACKEND_BITNET,
    .context_size = 2048,
    .temperature = 0.0f,
    .mode = NAGI_LLM_MODE_EXTRACTION
};

// Initialize
nagi_llm_init(llm, "path/to/model.gguf", &config);

// Set game dictionary
nagi_llm_set_dictionary(llm, dictionary_data, dictionary_size);

// Extract words (translate to English)
const char *result = nagi_llm_extract_words(llm, "mira el castillo");
// result: "look castle"

// Semantic matching
int word_ids[] = {10, 25};  // "look", "castle"
int matches = nagi_llm_matches_expected(llm, "mira la fortaleza", word_ids, 2);
// matches: 1 (semantically equivalent)

// Generate response (translate to user's language)
char response[256];
nagi_llm_generate_response(llm, "You see a castle.", "mira", response, 256);
// response: "Ves un castillo."

// Cleanup
nagi_llm_destroy(llm);
```

## LLM Operation Modes

### EXTRACTION Mode (Fast)
Extracts verb+noun from user input in any language to English.
- **Latency:** ~50-100ms (llamacpp), ~10-20ms (BitNet)

### SEMANTIC Mode (Precise)
Checks if user input semantically matches expected command.
- **Latency:** ~100-200ms (llamacpp), ~20-40ms (BitNet)

### DISABLED Mode
Falls back to original text parser only.

## Building

### Prerequisites
- CMake 3.14+
- C11 compiler
- For llamacpp: Metal (macOS) or CUDA (Linux/Windows)
- For BitNet: LLVM 18 (macOS ARM64)

### Build Commands

```bash
# With llama.cpp
cmake -DNAGI_LLM_ENABLE_LLAMACPP=ON ..
make

# With BitNet
cmake -DNAGI_LLM_ENABLE_BITNET=ON ..
make

# With both
cmake -DNAGI_LLM_ENABLE_LLAMACPP=ON -DNAGI_LLM_ENABLE_BITNET=ON ..
make
```

## Architecture

```
lib/nagi-llm/
├── include/
│   ├── nagi_llm.h          # Public API
│   ├── nagi_llm_context.h  # Context management
│   └── llm_utils.h         # Shared utilities
├── src/
│   ├── nagi_llm.c          # Factory and shared extraction
│   ├── nagi_llm_context.c  # Game context tracking
│   └── llm_utils.c         # Dictionary parsing, verb extraction
└── backends/
    ├── llamacpp/           # llama.cpp implementation
    │   └── nagi_llm_llamacpp.c
    └── bitnet/             # BitNet.cpp implementation
        └── nagi_llm_bitnet.c
```
