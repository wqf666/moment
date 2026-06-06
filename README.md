# 🎓 Moment - CPP大学校园论坛

<div align="center">

![Moment Logo](https://img.shields.io/badge/Moment-CPP%20University-667eea?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMDAgMTAwIj48dGV4dCB5PSIuOWVtIiBmb250LXNpemU9IjkwIj7wn46TPC90ZXh0Pjwvc3ZnPg==)

**记录美好时光 · CPP大学专属社交平台**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Frontend](https://img.shields.io/badge/frontend-Vanilla%20JS-yellow)](echo-client/)
[![Backend](https://img.shields.io/badge/backend-C%2B%2B%20%2B%20Drogon-green)](echo-server/)
[![AI](https://img.shields.io/badge/ai-llama.cpp-purple)](echo-ai/)

</div>

---

## 📖 项目简介

**Moment** 是专为大学生打造的校园社交平台，集动态分享、社交互动、AI助手于一体，帮助同学们记录和分享在CPP大学的美好校园生活。

### ✨ 核心功能

- 🏠 **校园广场** - 浏览全校同学的最新动态
- 💫 **关注流** - 只看你关心的同学和话题
- ✍️ **发布动态** - 分享学习、生活、活动点滴
- 👤 **个人空间** - 展示你的CPP大学身份和风采
- 🤖 **AI助手** - 解答校园相关问题，提供学习和生活建议
- 💬 **评论互动** - 与同学们畅所欲言
- 👥 **关注系统** - 建立你的校园社交圈

---

## 🚀 快速开始

### 前置要求

- **Node.js** >= 16.0 (前端开发)
- **CMake** >= 3.14 (后端编译)
- **C++ Compiler** with C++17 support
- **Git** (版本控制)

### 安装步骤

#### 1. 克隆项目

```bash
git clone https://github.com/wqf666/moment.git
cd moment
```

#### 2. 初始化子模块

```bash
git submodule update --init --recursive
```

#### 3. 启动后端服务

```bash
cd echo-server
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./echo-server
```

后端服务将运行在 `http://127.0.0.1:8080`

#### 4. 启动AI服务（可选）

```bash
cd ../..
cd echo-server
bash scripts/start_ai.sh
```

AI服务将运行在 `http://127.0.0.1:18080`

> ⚠️ **注意**: 首次使用需要下载Qwen2.5模型文件到 `echo-ai/models/qwen2.5-1.5b/` 目录

#### 5. 启动前端开发服务器

```bash
cd ../..
cd echo-client
npm install
npm run dev
```

前端应用将运行在 `http://localhost:5173`

---

## 📁 项目结构

```
moment/
├── echo-client/          # 前端应用
│   ├── src/             # 源代码
│   │   ├── main.js      # 主应用逻辑
│   │   └── style.css    # 样式文件
│   ├── index.html       # HTML入口
│   ├── package.json     # 依赖配置
│   └── vite.config.js   # Vite配置
│
├── echo-server/         # 后端服务
│   ├── src/             # C++源代码
│   │   ├── main.cpp     # 主程序
│   │   ├── controllers/ # API控制器
│   │   └── services/    # 业务服务
│   ├── scripts/         # 启动脚本
│   ├── CMakeLists.txt   # CMake配置
│   └── API_DOC.md       # API文档
│
├── echo-ai/             # AI服务
│   ├── llama.cpp/       # LLM推理引擎（子模块）
│   └── models/          # 模型文件（不纳入版本控制）
│
├── .gitignore           # Git忽略配置
├── LICENSE              # MIT许可证
└── README.md            # 项目说明
```

---

## 🎨 技术栈

### 前端
- **框架**: Vanilla JavaScript (原生JS)
- **构建工具**: Vite 6.x
- **HTTP客户端**: Fetch API
- **样式**: CSS3 (渐变、动画、响应式)

### 后端
- **语言**: C++17
- **Web框架**: [Drogon](https://github.com/drogonframework/drogon)
- **数据库**: MySQL 8.0+
- **缓存**: Redis (可选)
- **构建系统**: CMake

### AI服务
- **推理引擎**: [llama.cpp](https://github.com/ggerganov/llama.cpp)
- **模型**: Qwen2.5-1.5B-Instruct (量化版)
- **API**: OpenAI兼容接口

---

## 🔧 开发指南

### 后端开发

#### 添加新的API端点

1. 在 `echo-server/src/controllers/` 创建控制器
2. 在路由注册文件中添加路由
3. 实现业务逻辑

示例:
```cpp
// 在控制器中添加
void MyController::handleNewFeature(const HttpRequestPtr& req, 
                                    std::function<void(const HttpResponsePtr&)>&& callback) {
    // 处理逻辑
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("{\"message\":\"success\"}");
    callback(resp);
}
```

#### 数据库初始化

```bash
# 执行统一的数据库初始化脚本
cd echo-server
mysql -u echo_user -p < sql/init.sql
```

SQL脚本统一放在 `echo-server/sql/` 目录。

### 前端开发

#### 添加新页面

1. 在 `main.js` 中添加渲染函数
2. 在导航栏添加标签按钮
3. 实现Tab切换逻辑

#### 修改样式

编辑 `src/style.css`，遵循紫色主题规范

---

## 📝 API文档

完整API文档请查看：[`echo-server/API_DOC.md`](echo-server/API_DOC.md)

主要接口包括：

| 模块 | 接口 |
|---|---|
| 认证 | `/api/auth/register`, `/api/auth/login` |
| 帖子 | `/api/posts`, `/api/posts/latest`, `/api/feed/following` |
| 互动 | `/api/posts/like/toggle`, `/api/posts/comment` |
| 关注 | `/api/follows/toggle`, `/api/follows/followers`, `/api/follows/following` |
| 用户 | `/api/users/me`, `/api/users/profile/update` |
| AI | `/api/ai/chat`, `/api/ai/conversations`, `/api/ai/messages` |
| 媒体 | `/api/upload/media`, `/api/media/file/{fileName}` |

---

## ❓ 常见问题

### 1. 可执行文件找不到

请检查 CMake 生成的可执行文件名是否为 `echo-server`。

### 2. 数据库连接失败

确认MySQL服务已启动，并检查连接配置：
- Host: `127.0.0.1`
- Port: `3306`
- Database: `echo_app`
- User: `echo_user`
- Password: `123456`

可以通过以下命令测试连接：
```bash
mysql -h 127.0.0.1 -P 3306 -u echo_user -p echo_app
```

### 3. AI服务启动失败

确保已下载模型文件到正确目录：
```bash
ls -la echo-ai/models/qwen2.5-1.5b/
```

---

## 📄 许可证

本项目采用 [MIT License](LICENSE) 开源协议

---

## 🙏 致谢

- [Drogon Framework](https://github.com/drogonframework/drogon) - 高性能C++ Web框架
- [llama.cpp](https://github.com/ggerganov/llama.cpp) - 高效的LLM推理引擎
- [Qwen](https://github.com/QwenLM/Qwen) - 阿里巴巴通义千问大语言模型
- [Vite](https://vitejs.dev/) - 下一代前端构建工具

---

<div align="center">

⭐ 如果这个项目对你有帮助，请给我们一个Star！

Made with ❤️ by Moment Team

</div>
