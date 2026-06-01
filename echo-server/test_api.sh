#!/usr/bin/env bash
set -euo pipefail

BASE="${BASE:-http://127.0.0.1:8080}"
TOKEN_1="${TOKEN_1:-demo_token_1}"

need_jq() {
  if command -v jq >/dev/null 2>&1; then
    jq .
  else
    cat
    echo
  fi
}

echo "1) Toggle follow: user 1 -> user 2"
curl -s -X POST "$BASE/api/follows/toggle" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_1" \
  -d '{"target_user_id":2}' | need_jq

echo "2) Followers of user 2"
curl -s "$BASE/api/follows/followers?user_id=2&page=1&page_size=20" \
  -H "Authorization: Bearer $TOKEN_1" | need_jq

echo "3) Following list of user 1"
curl -s "$BASE/api/follows/following?user_id=1&page=1&page_size=20" \
  -H "Authorization: Bearer $TOKEN_1" | need_jq

echo "4) Following feed of user 1"
curl -s "$BASE/api/feed/following?page=1&page_size=20" \
  -H "Authorization: Bearer $TOKEN_1" | need_jq

echo "5) Update current user profile"
curl -s -X POST "$BASE/api/users/profile/update" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_1" \
  -d '{"nickname":"EchoUser1","avatar_url":"/api/media/file?name=test.jpg","bio":"hello echo","cover_image_url":"/api/media/file?name=test.jpg"}' | need_jq

echo "6) User home"
curl -s "$BASE/api/users/home?user_id=1&page=1&page_size=10" \
  -H "Authorization: Bearer $TOKEN_1" | need_jq

echo "7) User media posts"
curl -s "$BASE/api/users/media?user_id=1&page=1&page_size=20" \
  -H "Authorization: Bearer $TOKEN_1" | need_jq

echo "8) My posts"
curl -s "$BASE/api/posts/mine?page=1&page_size=20" \
  -H "Authorization: Bearer $TOKEN_1" | need_jq

echo "9) Update post 1"
curl -s -X POST "$BASE/api/posts/update" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $TOKEN_1" \
  -d '{"post_id":1,"content":"updated content","image_url":""}' | need_jq