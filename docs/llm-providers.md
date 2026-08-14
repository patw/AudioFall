# LLM providers for summarization

AudioFall sends each summary step to an **OpenAI-compatible chat-completions endpoint**. Configure these values in **Settings → LLM**:

- **Base URL**: the provider's OpenAI-compatible API base URL
- **API Key**: the provider key
- **Model**: the provider's model identifier

AudioFall appends `/chat/completions` to the base URL and sends both `Authorization: Bearer <key>` and `api-key: <key>` headers. The model name and available context length depend on the provider.

## Hosted providers

These providers offer OpenAI-compatible endpoints and are natural choices for AudioFall:

| Provider | Base URL | Model value |
|---|---|---|
| OpenAI | `https://api.openai.com/v1` | The model ID from OpenAI's current API documentation, such as `gpt-4o-mini` when available |
| Fireworks AI | `https://api.fireworks.ai/inference/v1` | A Fireworks model ID, commonly in the form `accounts/fireworks/models/...` |
| Cerebras | `https://api.cerebras.ai/v1` | A currently available Cerebras model ID |
| xAI / Grok | `https://api.x.ai/v1` | A currently available `grok-...` model ID |

Provider model catalogs and pricing change over time, so use the model identifier shown in the provider's documentation or dashboard rather than copying an old example blindly.

### Anthropic / Claude

Anthropic's native API is not the same endpoint shape as AudioFall's OpenAI-compatible chat-completions client. To use Claude, use an OpenAI-compatible gateway or adapter that translates `/v1/chat/completions` requests to Anthropic's Messages API, then configure AudioFall with that gateway's base URL and key. Do not use `https://api.anthropic.com` directly as the AudioFall LLM URL unless a compatibility layer is in front of it.

## Run a model locally

Local inference keeps transcripts on the machine. Two approachable options are:

### Ollama

Install Ollama from [ollama.com](https://ollama.com/), download a model, and start it:

```bash
ollama pull llama3.1:8b
ollama serve
```

Ollama provides an OpenAI-compatible endpoint at:

```text
http://localhost:11434/v1
```

Use these AudioFall settings:

```text
Base URL: http://localhost:11434/v1
API Key: ollama
Model: llama3.1:8b
```

The API key is not used by a default local Ollama server, but AudioFall always sends one, so a placeholder such as `ollama` is convenient.

### llama.cpp

Build or download `llama-server` from [llama.cpp](https://github.com/ggml-org/llama.cpp), download a compatible GGUF model, and run:

```bash
llama-server \
  -m /path/to/model.gguf \
  --host 127.0.0.1 \
  --port 8080
```

Recent llama.cpp servers expose an OpenAI-compatible API. Use:

```text
Base URL: http://localhost:8080/v1
API Key: local
Model: the model name accepted by your llama-server build
```

The model must be suitable for instruction following and have enough context for the transcript plus the summary prompt. The local Whisper model described in [local-whisper.md](local-whisper.md) is separate from the LLM used here.

## Choosing a model

Summary quality depends on the model's instruction following, context window, and language support. For long meetings, make sure the provider or local server supports a context window large enough for the transcript and prompt. AudioFall runs each configured summary step against the transcript, so adding more steps results in more LLM requests and may increase cost or latency.

Keep API keys in the application's settings rather than committing them to the repository. Settings are stored in the operating system's application configuration location.
