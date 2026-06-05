# Moment - CUMT校园论坛 🎓

> 记录矿大美好时光 · 中国矿业大学专属社交平台

## 🚀 快速开始

### 一键启动（推荐）

```bash
./start.sh
```

### 手动启动

```bash
# 1. 后端
cd echo-server/build && cmake .. && make && ./echo_server

# 2. AI服务（可选）
cd ../.. && bash scripts/start_ai.sh

# 3. 前端
cd ../../echo-client && npm install && npm run dev
```

## 📖 文档导航

| 文档 | 说明 |
|------|------|
| [README.md](README.md) | 项目介绍和安装指南 |
| [DEVELOPMENT.md](DEVELOPMENT.md) | 技术开发文档 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 贡献者指南 |
| [CHANGELOG.md](CHANGELOG.md) | 版本历史 |
| [SECURITY.md](SECURITY.md) | 安全策略 |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | 行为准则 |
| [GITHUB_CHECKLIST.md](GITHUB_CHECKLIST.md) | 上传检查清单 |
| [GITHUB_READY.md](GITHUB_READY.md) | GitHub优化完整指南 |

## 🌟 核心功能

- 🏠 校园广场
- 💫 关注动态  
- ✍️ 发布动态
- 👤 个人空间
- 🤖 AI助手
- 💬 评论互动
- 👥 关注系统

## 🛠️ 技术栈

- **前端**: Vanilla JS + Vite
- **后端**: C++17 + Drogon
- **数据库**: PostgreSQL
- **AI**: llama.cpp + Qwen2.5

## 📄 许可证

[MIT License](LICENSE)

---

**Made with ❤️ by CUMT Students**
