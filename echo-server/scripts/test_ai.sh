#!/usr/bin/env bash

curl http://127.0.0.1:18080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "local-model",
    "messages": [
      {
        "role": "user",
        "content": "你好，介绍一下你自己。"
      }
    ],
    "max_tokens": 200
  }'
