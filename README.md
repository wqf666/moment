# 🎓 Moment - CUMT校园论坛

<div align="center">

![Moment Logo](https://img.shields.io/badge/Moment-CUMT-667eea?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMDAgMTAwIj48dGV4dCB5PSIuOWVtIiBmb250LXNpemU9IjkwIj7wn46TPC90ZXh0Pjwvc3ZnPg==)

**记录矿大美好时光 · 中国矿业大学专属社交平台**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Frontend](https://img.shields.io/badge/frontend-Vanilla%20JS-yellow)](echo-client/)
[![Backend](https://img.shields.io/badge/backend-C%2B%2B%20%2B%20Drogon-green)](echo-server/)
[![AI](https://img.shields.io/badge/ai-llama.cpp-purple)](echo-ai/)

</div>

---

## 📖 项目简介

**Moment** 是专为中国矿业大学（CUMT）学生打造的校园社交平台，集动态分享、社交互动、AI助手于一体，帮助同学们记录和分享在矿大的美好校园生活。

### ✨ 核心功能

- 🏠 **校园广场** - 浏览全校同学的最新动态
- 💫 **关注流** - 只看你关心的同学和话题
- ✍️ **发布动态** - 分享学习、生活、活动点滴
- 👤 **个人空间** - 展示你的矿大身份和风采
- 🤖 **AI助手** - 解答矿大相关问题，提供学习和生活建议
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
git clone https://github.com/your-username/moment-cumt.git
cd moment-cumt
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
./echo_server
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
moment-cumt/
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
│   └── docs/            # API文档
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
- **数据库**: PostgreSQL / MySQL
- **缓存**: Redis (可选)
- **构建系统**: CMake

### AI服务
- **推理引擎**: [llama.cpp](https://github.com/ggerganov/llama.cpp)
- **模型**: Qwen2.5-1.5B-Instruct (量化版)
- **API**: OpenAI兼容接口

---

## 🌈 主题设计

### 品牌色彩
- **主色调**: 紫色渐变系
  - 起始色: `#667eea` (淡紫蓝)
  - 中间色: `#764ba2` (深紫)
  - 结束色: `#f093fb` (粉紫)

### 设计理念
- 🎓 **学术氛围**: 学位帽标识体现大学属性
- ⚡ **青春活力**: 紫色渐变象征年轻人的朝气
- 🎯 **现代简约**: 毛玻璃效果和圆角设计
- 💝 **亲切友好**: Emoji和温暖文案提升体验

---

## 📸 界面预览

### 主要页面
- **登录/注册页** - 简洁友好的用户入口
- **校园广场** - 瀑布流展示最新动态
- **个人空间** - 个性化资料展示
- **AI助手** - 智能对话界面

*(截图待添加)*

---

## 🔧 开发指南

### 后端开发

#### 添加新的API端点

1. 在 `echo-server/src/controllers/` 创建控制器
2. 在 `main.cpp` 中注册路由
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

#### 数据库迁移

SQL脚本放在 `echo-server/docs/` 目录

### 前端开发

#### 添加新页面

1. 在 `main.js` 中添加渲染函数
2. 在导航栏添加标签按钮
3. 实现Tab切换逻辑

#### 修改样式

编辑 `src/style.css`，遵循紫色主题规范

---

## 🧪 测试

### API测试

```bash
cd echo-server
bash test_api.sh
```

### AI接口测试

```bash
cd echo-server
bash scripts/test_ai.sh
```

---

## 📝 API文档

完整的API文档请查看: [echo-server/docs/API_DOC.md](echo-server/docs/API_DOC.md)

### 主要接口

| 端点 | 方法 | 描述 |
|------|------|------|
| `/api/auth/login` | POST | 用户登录 |
| `/api/auth/register` | POST | 用户注册 |
| `/api/posts` | GET | 获取帖子列表 |
| `/api/posts` | POST | 创建帖子 |
| `/api/comments` | POST | 发表评论 |
| `/api/users/:id/follow` | POST | 关注用户 |
| `/api/ai/chat` | POST | AI聊天 |
| `/api/ai/conversations` | GET | 获取对话列表 |

---

## 🤝 贡献指南

我们欢迎所有形式的贡献！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 贡献规范

- 遵循现有的代码风格
- 添加必要的注释和文档
- 确保测试通过
- 更新README（如需要）

---

## 📄 许可证

本项目采用 [MIT License](LICENSE) 开源协议

---

## 👥 团队

**Moment Team** - 中国矿业大学学生开发团队

---

## 🙏 致谢

- [Drogon Framework](https://github.com/drogonframework/drogon) - 高性能C++ Web框架
- [llama.cpp](https://github.com/ggerganov/llama.cpp) - 高效的LLM推理引擎
- [Qwen](https://github.com/QwenLM/Qwen) - 阿里巴巴通义千问大语言模型
- [Vite](https://vitejs.dev/) - 下一代前端构建工具

---

## 📞 联系方式

- 📧 Email: your-email@cumt.edu.cn
- 💬 QQ群: [加入讨论](#)
- 🐛 Issues: [GitHub Issues](https://github.com/your-username/moment-cumt/issues)

---

<div align="center">

**Made with ❤️ by CUMT Students**

⭐ 如果这个项目对你有帮助，请给我们一个Star！

</div>
