# AGI-Llama: AI-Enhanced Adventure Game Interpreter

**A Modern Fork of NAGI with LLM Integration**
**WELCOME TO THE RETRO FUTURE!!!**

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
- **Modular Architecture**: Clean separation between game engine and AI layer. The llm code is encapsulated in its own lib and could be easily integrated in other projects like SCUMMVM or Sarien.

## Current Developers

  * [Nick Sonneveld][1] - Original NAGI author
  * [Gareth McMullin][2] - Linux port
  * [Claudio Matsuoka][3] - Project support
  * [Ritchie Swann][4] - OS X port, SDL2 upgrade
  * [Juan Alfonso Sierra][5] - SDL3 and SDL_TTF port, LLM integration

## LLM Features (Experimental)

### Supported Backends

1. **llama.cpp** ✅
   - GGUF models with GPU acceleration
   - Models: Llama 3.2 3B, Qwen 2.5 7B, Gemma 3 4B
   - Requires model download (2-5GB)

2. **BitNet** 🚧 - Experimental
   - 1.58-bit quantized models
   - Needs optimization

3. **Cloud API** ✅
   - OpenAI-compatible endpoints
   - Supports: Hugging Face, Groq, Cerebras, OpenAI, Ollama...
   - No local setup required, just API key

### Known Limitations

- **Blocking I/O**: LLM requests block the main game thread. Async implementation would require extensive NAGI refactoring
- **Model Quality**: Small models (3B-8B params) can produce incoherent responses with high temperature
- **Performance**: BitNet backend needs optimization

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

## New scripts commands (WIP)

- Added new **update.context** command to to be able to send additional context to the LLM. This require edit and reexport the original game with a custom version of the AgiStudio tool that I am working on.

## Build Requirements

- CMake 3.14+
- C compiler (GCC/Clang/MSVC)
- SDL3 (fetched automatically)
- CURL (for cloud backend)

### Optional LLM Dependencies

- **llama.cpp**: Built automatically if enabled
- **BitNet**: Built automatically if enabled
- **Cloud API**: Only needs CURL

## Project Structure

```
agi-llm/
├── llm_config_example.ini      # Configuration template
├── lib/nagi-llm/               # LLM integration library
│   ├── backends/               # Backend implementations
│   │   ├── llamacpp/           # Local llama.cpp backend
│   │   ├── bitnet/             # BitNet quantized backend
│   │   └── cloud/              # Cloud API backend
│   └── src/
│       └── llm_config_parser.c # Unified configuration parser
└── src/                        # Main NAGI source
```

## Configuration

### Unified Configuration (llm_config.ini)

All backends use a single configuration file `llm_config.ini`:

```ini
[common]
temperature_extraction = 0.0       # Always 0.0 for deterministic extraction
temperature_creative_base = 0.3    # Base temperature for responses
temperature_creative_offset = 0.2  # Random variation range
max_tokens = 512
verbose = 1

[llamacpp]
context_size = 4096
use_gpu = 1
# ... llamacpp specific settings

[bitnet]
context_size = 4096
use_gpu = 0
# ... bitnet specific settings

[cloud]
api_url = https://api-inference.huggingface.co/models/meta-llama/Llama-3.2-3B-Instruct/v1/chat/completions
api_key = YOUR_API_KEY_HERE
model = meta-llama/Llama-3.2-3B-Instruct
```

## How to Build

### Quick Start 

#### Local llama.cpp backend

```bash
mkdir build && cd build
cmake .. -DNAGI_LLM_ENABLE_LLAMACPP=ON
make
./run.sh /path/to/game
```

#### BitNet backend

```bash
mkdir build && cd build
cmake .. -DNAGI_LLM_ENABLE_BITNET=ON
make
./run.sh /path/to/game
```

#### Hugging Face Cloud backend

```bash
# 1. Get free API key from https://huggingface.co/settings/tokens
# 2. Edit llm_config.ini and set your api_key
mkdir build && cd build
cmake .. -DNAGI_LLM_ENABLE_CLOUD_API=ON
make
./run.sh /path/to/game
```

#### No LLM (classic NAGI)

```bash
cmake ..
```

## How to Run

After building, you need AGI game files to play:

```bash
# Linux/macOS
./run.sh /path/to/game/directory

# Windows
run.bat C:\path\to\game\directory
```

## Systems Supported

- **macOS** (Metal)
- **Linux** (Vulkan)
- **Windows** (Vulkan)

## Build Status

| Platform | Status | Notes |
| --- | --- | --- |
| macOS | ✅ Works | Metal renderer |
| Linux | ⚠️ Builds | Testing vulkan support |
| Windows | ✅ Works | Vulkan |

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

This LLM integration was developed with assistance from several AI coding tools.

---

**Note**: This is an experimental research project developed with AI assistance. The LLM integration is a proof-of-concept to explore AI in retro gaming. Production use is not recommended.

[1]: mailto:sonneveld.at.hotmail.com
[2]: mailto:g_mcm.at.mweb.co.za
[3]: mailto:claudio.at.helllabs.org
[4]: mailto:ritchieswann@gmail.com
[5]: mailto:jalfonsosm@yahoo.es
