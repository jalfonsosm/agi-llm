# NAGI-LLM: Abstract LLM Interface Library

NAGI-LLM is a modular, backend-agnostic library for integrating Large Language Models into the NAGI AGI game engine. It provides a unified interface for multiple LLM backends, enabling natural language understanding in classic adventure games.

**⚠️ Experimental Research Project**: This is a rapid prototype to investigate AI integration in retro games. Many improvements remain.

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
| **llama.cpp** | ✅ Tested | Fast | ~4GB | Optional | ✅ Yes | High |
| **BitNet** | 🚧 WIP | Slow* | ~1.1GB | No | ✅ Yes | Good |
| **Cloud API** | 🚧 WIP | Network | Minimal | N/A | ❌ No | Excellent |

*BitNet currently slower than expected, needs optimization

### 1. llama.cpp Backend ✅

High-performance local inference using llama.cpp with full GPU acceleration.

**Features:**
- Metal GPU acceleration on macOS
- GGUF model support
- Advanced KV cache management
- Multiple model support (Llama 3.2, Qwen 2.5, etc.)

**Default Model:** Llama 3.2 3B Instruct Q4_K_M (2.3GB)

**Enable:** `-DNAGI_LLM_ENABLE_LLAMACPP=ON`

### 2. BitNet Backend 🚧

Ultra-efficient 1.58-bit quantized models using Microsoft's BitNet.cpp.

**Status:** Prototype - needs performance optimization

**Features:**
- 1.58-bit ternary quantization
- Minimal memory usage (~1.1GB)
- CPU-only inference
- No GPU required

**Known Issues:**
- Currently slower than llama.cpp (needs optimization)
- Limited model selection

**Enable:** `-DNAGI_LLM_ENABLE_BITNET=ON`

### 3. Cloud API Backend 🚧

OpenAI-compatible API support for cloud LLM services.

**Status:** Prototype - basic functionality working

**Supported Services:**
- Cerebras (free tier available)
- Groq (free tier available)
- OpenAI
- Local Ollama

**Enable:** `-DNAGI_LLM_ENABLE_CLOUD_API=ON`

## Quick Start

```c
#include "nagi_llm.h"

// Create LLM instance
nagi_llm_t *llm = nagi_llm_create(NAGI_LLM_BACKEND_LLAMACPP);

// Configure
nagi_llm_config_t config = {
    .backend = NAGI_LLM_BACKEND_LLAMACPP,
    .context_size = 4096,
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
- **Use case:** Quick translation for parser matching
- **Latency:** ~50-100ms (llamacpp)

### SEMANTIC Mode (Precise)
Checks if user input semantically matches expected command.
- **Use case:** Fuzzy matching when exact translation fails
- **Latency:** ~100-200ms (llamacpp)

### DISABLED Mode
Falls back to original text parser only.

## Known Limitations

### Blocking I/O
LLM requests block the main game thread. Async implementation would require extensive NAGI refactoring. For simplicity, this was not implemented.

### Model Quality
Small models (3B-8B params) can produce incoherent responses with high temperature settings. Use temperature 0.0-0.3 for best results.

### Performance
- BitNet backend needs optimization (currently slower than llama.cpp)
- No streaming support yet
- KV cache management could be improved

## Future Improvements

### Performance & Architecture
- [ ] **Async/non-blocking requests**: Requires major NAGI refactoring
- [ ] **BitNet optimization**: Investigate performance bottlenecks
- [ ] **Streaming**: Real-time token generation

### User Experience
- [ ] **Speech-to-text**: Integrate sherpa-onnx for voice input
- [ ] **Text-to-speech**: Add voice output for responses
- [ ] **In-game hints**: Allow LLM to provide contextual hints to players

### Model & Context
- [ ] **LoRA fine-tuning**: Train custom models on AGI game walkthroughs/guides
- [ ] **Better context**: Use existing game state logic for richer LLM context
- [ ] **Larger models**: Support for 13B+ models
- [ ] **Better prompts**: Improve extraction and generation quality

## Building

### Prerequisites
- CMake 3.14+
- C11 compiler
- SDL3 (fetched automatically)
- CURL (for cloud backend)

### Build Commands

```bash
# With llama.cpp (recommended)
cmake -DNAGI_LLM_ENABLE_LLAMACPP=ON ..
make

# With BitNet (experimental)
cmake -DNAGI_LLM_ENABLE_BITNET=ON -DNAGI_LLM_ENABLE_LLAMACPP=OFF ..
make

# With Cloud API (experimental)
cmake -DNAGI_LLM_ENABLE_CLOUD_API=ON -DNAGI_LLM_ENABLE_LLAMACPP=OFF ..
make
```

### Model Selection (llama.cpp)

```bash
# Llama 3.2 3B (default, 2.3GB)
cmake .. -DMODEL_NAME=LLAMA3

# Llama 3.1 8B (better multilingual, 4.9GB)
cmake .. -DMODEL_NAME=LLAMA3_8B

# Qwen 2.5 7B (excellent multilingual, 4.8GB)
cmake .. -DMODEL_NAME=QWEN2
```

### Cloud API Setup

1. Copy config: `cp cloud_config_example.ini cloud_config.ini`
2. Get free API key from [Cerebras](https://console.cerebras.ai)
3. Edit `cloud_config.ini` with your key
4. Build with cloud backend enabled

## Architecture

```
lib/nagi-llm/
├── include/
│   ├── nagi_llm.h          # Public API
│   ├── nagi_llm_context.h  # Context management
│   └── llm_utils.h         # Shared utilities
├── src/
│   ├── nagi_llm.c          # Factory and interface
│   ├── nagi_llm_context.c  # Game context tracking
│   └── llm_utils.c         # Dictionary parsing, verb extraction
└── backends/
    ├── llamacpp/           # llama.cpp implementation ✅
    │   ├── nagi_llm_llamacpp.c
    │   └── nagi_llm_llamacpp.h
    ├── bitnet/             # BitNet.cpp implementation 🚧
    │   ├── nagi_llm_bitnet.c
    │   └── nagi_llm_bitnet.h
    ├── cloud/              # Cloud API implementation 🚧
    │   ├── nagi_llm_cloud.c
    │   ├── nagi_llm_cloud_impl.c
    │   └── nagi_llm_cloud.h
    └── llama_common.h      # Shared llama.cpp utilities
```

## Acknowledgments

- llama.cpp by Georgi Gerganov
- BitNet.cpp by Microsoft Research
- NAGI by Nick Sonneveld

### AI-Assisted Development

This library was developed with assistance from AI coding tools:
- Amazon Q Developer
- Claude (Anthropic)
- DeepSeek
- GitHub Copilot
- Gemini AI (Google)

## License

X11 License (compatible with GPL)

Copyright (c) 2024 Juan Alfonso Sierra

---

**Note**: This is an experimental research project developed with AI assistance. The LLM integration is a proof-of-concept to explore AI in retro gaming. Production use is not recommended.
