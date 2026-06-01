#!/bin/bash
BASE_URL="http://127.0.0.1:8080"

echo "压测最新帖子列表"
# 加 -L 参数 ↓
wrk -t4 -c100 -d30s -L "$BASE_URL/api/posts/latest?page=1&page_size=20"

echo "压测用户主页"
wrk -t4 -c100 -d30s -L "$BASE_URL/api/users/home?user_id=1&page=1&page_size=10"

echo "压测帖子详情"
wrk -t4 -c100 -d30s -L "$BASE_URL/api/posts/detail?post_id=1"