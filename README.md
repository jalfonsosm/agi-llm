# AGI-Llama: AI-Enhanced Adventure Game Interpreter

**A Modern Fork of NAGI with LLM Integration**

*Original NAGI by Nick Sonneveld | LLM Integration by Juan Alfonso Sierra*

## What is AGI-Llama?

AGI-Llama is an experimental fork of NAGI (New Adventure Game Interpreter) that brings modern AI capabilities to classic 1980s Sierra adventure games. By integrating Large Language Models, it transforms how players interact with beloved titles like Space Quest, King's Quest, and Leisure Suit Larry.

### Why Fork NAGI?

With extensive modernization including SDL3 support, Unicode text rendering (SDL_ttf), and a complete LLM integration layer, this project has evolved significantly from the original NAGI. The rebranding reflects these substantial changes while honoring the original work.

### Key Features

- 🌍 **Multilingual Gameplay**: Play in any language - Spanish, French, Japanese, etc.
- 🤖 **Natural Language Input**: Type commands naturally, not just "verb noun"
- 💬 **AI-Powered Responses**: Game responses translated to your language
- 🎮 **Original Game Logic**: Preserves authentic AGI game behavior
- 🖥️ **Modern Stack**: SDL3, Unicode support, GPU acceleration

![AGI-Llama in Action](./media/agiEnhanced.gif)

**⚠️ Experimental Research Project**: This is a rapid prototype to investigate AI integration in retro games. Many improvements remain.

## What's New in AGI-Llama?

- **SDL3 Support**: Modernized from SDL2 to SDL3 for better performance
- **Unicode Text Rendering**: SDL_ttf integration for multilingual display
- **LLM Integration**: Experimental AI-powered natural language processing
- **Multi-backend**: Support for llama.cpp, BitNet, and Cloud APIs
- **Modular Architecture**: Clean separation between game engine and AI layer

## Current Developers

  * [Nick Sonneveld][1] - Original NAGI author
  * [Gareth McMullin][2] - Linux port
  * [Claudio Matsuoka][3] - Project support
  * [Ritchie Swann][4] - OS X port, SDL2 upgrade
  * Juan Alfonso Sierra - LLM integration experiment

## LLM Features (Experimental)

### Supported Backends

1. **llama.cpp** ✅ - Tested and working
   - Local inference with GGUF models
   - GPU acceleration (Metal on macOS)
   - Models: Llama 3.2 3B, Llama 3.1 8B, Qwen 2.5 7B

2. **BitNet** 🚧 - Prototype (WIP)
   - 1.58-bit quantized models
   - Needs optimization (currently slower than expected)

3. **Cloud API** 🚧 - Prototype (WIP)
   - OpenAI-compatible endpoints
   - Supports: Cerebras, Groq, OpenAI, local Ollama

### Known Limitations

- **Blocking I/O**: LLM requests block the main game thread. Async implementation would require extensive NAGI refactoring
- **Model Quality**: Small models (3B-8B params) can produce incoherent responses with high temperature
- **Performance**: BitNet backend needs optimization
- **Limited Testing**: Only llama.cpp backend is production-ready

### Future Improvements

**Performance & Architecture:**
- [ ] Async/non-blocking LLM requests
- [ ] BitNet performance optimization

**User Experience:**
- [ ] Speech-to-text input (e.g., sherpa-onnx)
- [ ] Text-to-speech output
- [ ] In-game hints system (LLM provides contextual help)

**Model & Context:**
- [ ] LoRA fine-tuning on AGI game walkthroughs
- [ ] Better context management using game state
- [ ] Larger/better models support
- [ ] Better prompt engineering

## Build Requirements

- CMake 3.14+
- C compiler (GCC/Clang/MSVC)
- SDL3 (fetched automatically)
- CURL (for cloud backend)

### Optional LLM Dependencies

- **llama.cpp**: Built automatically if enabled
- **BitNet**: Built automatically if enabled
- **Cloud API**: Only needs CURL

## How to Build

### Linux/macOS

```bash
# Basic build (no LLM)
mkdir build && cd build
cmake ..
make

# With llama.cpp backend (default)
cmake .. -DNAGI_LLM_ENABLE_LLAMACPP=ON
make

# With BitNet backend
cmake .. -DNAGI_LLM_ENABLE_BITNET=ON -DNAGI_LLM_ENABLE_LLAMACPP=OFF
make

# With Cloud API backend
cmake .. -DNAGI_LLM_ENABLE_CLOUD_API=ON -DNAGI_LLM_ENABLE_LLAMACPP=OFF
make
```

### Model Selection (llama.cpp)

Choose model via CMake:

```bash
# Llama 3.2 3B (default, 2.3GB)
cmake .. -DMODEL_NAME=LLAMA3

# Llama 3.1 8B (better multilingual, 4.9GB)
cmake .. -DMODEL_NAME=LLAMA3_8B

# Qwen 2.5 7B (excellent multilingual, 4.8GB)
cmake .. -DMODEL_NAME=QWEN2

# Gemma 3 4B
cmake .. -DMODEL_NAME=GEMMA3

# Phi-3 4B
cmake .. -DMODEL_NAME=PHI3
```

### Cloud API Setup

1. Copy config: `cp cloud_config_example.ini cloud_config.ini`
2. Get free API key from [Cerebras](https://console.cerebras.ai)
3. Edit `cloud_config.ini` with your key
4. Build with cloud backend enabled

## Systems Supported

- **macOS** (Apple Silicon & Intel)
- **Linux** (x86_64, ARM64)
- **Windows** (experimental)

## License

NAGI's source is released under the X11 license (compatible with GPL).

    Copyright (c) 2001 - 2024 Nick Sonneveld, Gareth McMullin, Ritchie Swann

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, provided that the above copyright notice(s) and this
    permission notice appear in all copies of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.

## Acknowledgments

- Original NAGI by Nick Sonneveld
- SDL3 port contributions
- llama.cpp by Georgi Gerganov
- BitNet.cpp team
- AGI game preservation community

### AI-Assisted Development

This LLM integration was developed with assistance from AI coding tools:
- Amazon Q Developer
- Claude (Anthropic)
- DeepSeek
- GitHub Copilot
- Gemini AI (Google)

### Credits

- **Juan Alfonso Sierra** - LLM integration, architecture, and experimentation

---

**Note**: This is an experimental research project developed with AI assistance. The LLM integration is a proof-of-concept to explore AI in retro gaming. Production use is not recommended.

[1]: mailto:sonneveld.at.hotmail.com
[2]: mailto:g_mcm.at.mweb.co.za
[3]: mailto:claudio.at.helllabs.org
[4]: mailto:ritchieswann@gmail.com
