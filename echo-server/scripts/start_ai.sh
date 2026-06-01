#!/usr/bin/env bash
set -e

WORKSPACE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

LLAMA_SERVER="$WORKSPACE_ROOT/echo-ai/llama.cpp/build/bin/llama-server"
MODEL_PATH="$WORKSPACE_ROOT/echo-ai/models/qwen2.5-1.5b/qwen2.5-1.5b-instruct-q4_k_m.gguf"

exec "$LLAMA_SERVER" \
  -m "$MODEL_PATH" \
  --host 127.0.0.1 \
  --port 18080 \
  -c 2048
