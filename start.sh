#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BACKEND_DIR="$ROOT_DIR/echo-server"
FRONTEND_DIR="$ROOT_DIR/echo-client"
BACKEND_BUILD_DIR="$BACKEND_DIR/build"
BACKEND_BIN="$BACKEND_BUILD_DIR/echo-server"

FRONTEND_PORT="${FRONTEND_PORT:-5173}"
START_AI="${START_AI:-0}"

PIDS=()

log() {
  printf '\033[1;32m[Moment]\033[0m %s\n' "$1"
}

warn() {
  printf '\033[1;33m[Warn]\033[0m %s\n' "$1"
}

err() {
  printf '\033[1;31m[Error]\033[0m %s\n' "$1" >&2
}

cleanup() {
  log "正在停止服务..."

  for pid in "${PIDS[@]:-}"; do
    if kill -0 "$pid" >/dev/null 2>&1; then
      kill "$pid" >/dev/null 2>&1 || true
    fi
  done
}

trap cleanup EXIT INT TERM

init_submodules() {
  if [ -f "$ROOT_DIR/.gitmodules" ]; then
    log "初始化 Git 子模块..."
    git -C "$ROOT_DIR" submodule update --init --recursive
  fi
}

build_backend() {
  log "编译后端..."

  mkdir -p "$BACKEND_BUILD_DIR"

  cmake -S "$BACKEND_DIR" -B "$BACKEND_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$BACKEND_BUILD_DIR" -j

  if [ ! -x "$BACKEND_BIN" ]; then
    err "未找到后端可执行文件：$BACKEND_BIN"
    err "请确认 echo-server/CMakeLists.txt 里是：add_executable(echo-server ...)"
    exit 1
  fi
}

start_backend() {
  log "启动后端：http://127.0.0.1:8080"

  (
    cd "$BACKEND_BUILD_DIR"
    "$BACKEND_BIN"
  ) &

  PIDS+=("$!")

  sleep 1
}

install_frontend_deps() {
  if [ ! -d "$FRONTEND_DIR/node_modules" ]; then
    log "安装前端依赖..."
    npm --prefix "$FRONTEND_DIR" install
  fi
}

start_frontend() {
  log "启动前端：http://localhost:$FRONTEND_PORT"

  npm --prefix "$FRONTEND_DIR" run dev -- --host 0.0.0.0 --port "$FRONTEND_PORT" &

  PIDS+=("$!")
}

start_ai_optional() {
  if [ "$START_AI" != "1" ]; then
    warn "AI 服务默认不启动。如需启动，请使用：START_AI=1 ./start.sh"
    return
  fi

  local LLAMA_SERVER="$ROOT_DIR/echo-ai/llama.cpp/build/bin/llama-server"
  local MODEL_PATH="$ROOT_DIR/echo-ai/models/qwen2.5-1.5b/qwen2.5-1.5b-instruct-q4_k_m.gguf"

  if [ ! -x "$LLAMA_SERVER" ]; then
    warn "未找到 llama-server：$LLAMA_SERVER"
    return
  fi

  if [ ! -f "$MODEL_PATH" ]; then
    warn "未找到模型文件：$MODEL_PATH"
    return
  fi

  log "启动 AI 服务：http://127.0.0.1:18080"

  "$LLAMA_SERVER" \
    -m "$MODEL_PATH" \
    -c 2048 \
    -t 4 \
    --host 127.0.0.1 \
    --port 18080 &

  PIDS+=("$!")
}

main() {
  init_submodules
  build_backend
  start_ai_optional
  start_backend
  install_frontend_deps
  start_frontend

  log "开发环境已启动。"
  log "后端：http://127.0.0.1:8080"
  log "前端：http://localhost:$FRONTEND_PORT"
  log "按 Ctrl+C 停止全部服务。"

  wait
}

main "$@"