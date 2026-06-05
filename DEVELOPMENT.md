# 开发文档

本文档面向开发者，提供详细的技术说明和开发指南。

## 📋 目录

- [架构概览](#架构概览)
- [技术栈详解](#技术栈详解)
- [数据库设计](#数据库设计)
- [API规范](#api规范)
- [前端架构](#前端架构)
- [后端架构](#后端架构)
- [AI服务集成](#ai服务集成)
- [部署指南](#部署指南)

---

## 架构概览

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│   Browser   │◄────►│ Echo Client  │◄────►│ Echo Server │
│             │ HTTP │ (Vite + JS)  │ REST │ (C++ Drogon)│
└─────────────┘      └──────────────┘      └──────┬──────┘
                                                  │
                                           ┌──────▼──────┐
                                           │   Database  │
                                           │   (MySQL)   │
                                           └─────────────┘
                                                  │
                                           ┌──────▼──────┐
                                           │  AI Service │
                                           │ (llama.cpp) │
                                           └─────────────┘
```

### 组件说明

1. **Echo Client**: 前端单页应用
2. **Echo Server**: 后端REST API服务
3. **Database**: 持久化存储
4. **AI Service**: 大语言模型推理服务

---

## 技术栈详解

### 前端技术

| 技术 | 版本 | 用途 |
|------|------|------|
| Vanilla JS | ES6+ | 核心逻辑 |
| Vite | 6.x | 构建工具 |
| CSS3 | - | 样式和动画 |
| Fetch API | - | HTTP请求 |

**为什么选择原生JS？**
- 轻量级，无框架开销
- 学习成本低
- 完全控制DOM操作
- 适合小型项目

### 后端技术

| 技术 | 版本 | 用途 |
|------|------|------|
| C++ | 17 | 编程语言 |
| Drogon | 最新 | Web框架 |
| MySQL | 8.0+ | 关系数据库（统一使用） |
| Redis | 6+ | 缓存（可选） |
| CMake | 3.14+ | 构建系统 |

**为什么选择Drogon？**
- 高性能异步框架
- 内置ORM支持
- 完整的REST API支持
- C++生态友好

### AI技术

| 技术 | 版本 | 用途 |
|------|------|------|
| llama.cpp | 最新 | LLM推理引擎 |
| Qwen2.5 | 1.5B | 基础模型 |
| GGUF | Q4_K_M | 量化格式 |

---

## 数据库设计

### 数据库初始化

所有表结构定义已统一整理到 [`echo-server/sql/init.sql`](echo-server/sql/init.sql)

执行初始化：
```bash
mysql -u echo_user -p < echo-server/sql/init.sql
```

### ER图

```
┌──────────┐       ┌──────────┐       ┌──────────┐
│  users   │1     *│  posts   │1     *│ comments │
├──────────┤───────┼──────────┤───────┼──────────┤
│ id       │       │ id       │       │ id       │
│ username │       │ user_id  │       │ post_id  │
│ nickname │       │ content  │       │ user_id  │
│ password │       │ image_url│       │ content  │
│ avatar   │       │ created  │       │ created  │
│ bio      │       └──────────┘       └──────────┘
│ cover    │            │                    │
└──────────┘            │                    │
                        │1                  *│
                        └────────────────────┘
                              │
                        ┌─────▼─────┐     ┌──────────┐
                        │user_follows│    │post_likes│
                        ├───────────┤    ├──────────┤
                        │ follower  │    │ post_id  │
                        │ following │    │ user_id  │
                        └───────────┘    └──────────┘
                        
                        ┌──────────────────┐
                        │ai_conversations  │1     *┌──────────┐
                        ├──────────────────┤───────┤ai_messages│
                        │ user_id          │       ├──────────┤
                        │ title            │       │ role     │
                        │ created/updated  │       │ content  │
                        └──────────────────┘       └──────────┘
```

### 表结构详细说明

详见 [`echo-server/sql/init.sql`](echo-server/sql/init.sql)，包含以下7个表：

1. **users** - 用户表（主键、用户名、密码哈希、个人资料）
2. **posts** - 帖子表（内容、图片、点赞数、评论数）
3. **comments** - 评论表（评论内容、关联帖子和用户）
4. **user_follows** - 关注关系表（多对多关系）
5. **post_likes** - 点赞表（记录谁点了哪个帖子的赞）
6. **ai_conversations** - AI对话表（会话管理）
7. **ai_messages** - AI消息表（对话历史记录）

**注意**：项目统一使用MySQL，不再支持PostgreSQL。如需迁移，请参考init.sql中的MySQL语法。

---

## API规范

### 认证机制

使用Bearer Token进行身份验证：

```
Authorization: Bearer <token>
```

Token生成规则（开发环境）：
```
demo_token_{user_id}
```

生产环境应使用JWT或其他安全机制。

### 响应格式

成功响应：
```json
{
  "code": 0,
  "message": "success",
  "data": { ... }
}
```

错误响应：
```json
{
  "code": -1,
  "message": "错误描述",
  "data": null
}
```

### 主要端点

#### 认证相关

```
POST /api/auth/register
Content-Type: application/json

{
  "username": "testuser",
  "password": "password123",
  "nickname": "测试用户"
}

Response:
{
  "code": 0,
  "message": "success",
  "data": {
    "token": "demo_token_1",
    "user": { ... }
  }
}
```

```
POST /api/auth/login
Content-Type: application/json

{
  "username": "testuser",
  "password": "password123"
}
```

#### 帖子相关

```
GET /api/posts?page=1&limit=20
Authorization: Bearer demo_token_1

Response:
{
  "code": 0,
  "data": {
    "posts": [...],
    "total": 100,
    "page": 1,
    "limit": 20
  }
}
```

```
POST /api/posts
Authorization: Bearer demo_token_1
Content-Type: application/json

{
  "content": "今天矿大的樱花开了！",
  "media_url": "/uploads/xxx.jpg"
}
```

#### 评论相关

```
POST /api/comments
Authorization: Bearer demo_token_1
Content-Type: application/json

{
  "post_id": 1,
  "content": "太美了！"
}
```

#### 关注相关

```
POST /api/users/{user_id}/follow
Authorization: Bearer demo_token_1

DELETE /api/users/{user_id}/follow
Authorization: Bearer demo_token_1

GET /api/users/{user_id}/followers
Authorization: Bearer demo_token_1

GET /api/users/{user_id}/following
Authorization: Bearer demo_token_1
```

#### AI相关

```
POST /api/ai/chat
Authorization: Bearer demo_token_1
Content-Type: application/json

{
  "conversation_id": 1,
  "message": "图书馆几点关门？"
}

Response:
{
  "code": 0,
  "data": {
    "reply": "图书馆通常在晚上10点关门...",
    "conversation_id": 1
  }
}
```

```
GET /api/ai/conversations
Authorization: Bearer demo_token_1

GET /api/ai/messages?conversation_id=1
Authorization: Bearer demo_token_1
```

---

## 前端架构

### 文件结构

```
echo-client/
├── index.html          # HTML入口
├── package.json        # 依赖配置
├── vite.config.js      # Vite配置
└── src/
    ├── main.js         # 主应用逻辑 (~1200行)
    ├── style.css       # 全局样式 (~800行)
    └── assets/         # 静态资源
```

### 核心模块

#### 状态管理

```javascript
const state = {
  token: '',
  currentUser: null,
  currentTab: 'latest',
  posts: [],
  aiMessages: [],
  aiConversationId: null
}
```

#### 路由系统

基于Tab的单页路由：
```javascript
function openTab(tab) {
  state.currentTab = tab
  // 根据tab渲染不同内容
  if (tab === 'latest') loadLatestPosts()
  else if (tab === 'create') renderCreatePost()
  // ...
}
```

#### HTTP封装

```javascript
async function apiRequest(endpoint, options = {}) {
  const headers = {
    'Content-Type': 'application/json',
    'Authorization': `Bearer ${state.token}`
  }
  
  const response = await fetch(`/api${endpoint}`, {
    ...options,
    headers: { ...headers, ...options.headers }
  })
  
  return response.json()
}
```

### 样式规范

#### 命名约定

```
/* 组件类名 */
.post-card { }
.post-card__title { }  /* BEM元素 */
.post-card--featured { }  /* BEM修饰符 */

/* 功能类名 */
.primary-btn { }
.ghost-btn { }
.input { }
.textarea { }
```

#### 主题变量

虽然没有使用CSS变量，但保持颜色一致性：

```css
/* 主色渐变 */
background: linear-gradient(135deg, #667eea, #764ba2);

/* 焦点色 */
border-color: #667eea;
box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.15);
```

---

## 后端架构

### 文件结构

```
echo-server/
├── CMakeLists.txt      # CMake配置
├── src/
│   ├── main.cpp        # 主程序和路由注册
│   ├── controllers/    # API控制器
│   │   ├── AuthController.h/cpp
│   │   ├── PostController.h/cpp
│   │   ├── CommentController.h/cpp
│   │   ├── UserController.h/cpp
│   │   └── AiController.h/cpp
│   ├── services/       # 业务服务
│   │   ├── AuthService.h/cpp
│   │   ├── PostService.h/cpp
│   │   └── AiService.h/cpp
│   ├── models/         # 数据模型
│   └── utils/          # 工具函数
├── scripts/            # 启动脚本
└── docs/               # 文档
```

### 核心组件

#### 路由注册 (main.cpp)

```cpp
// 认证路由
app.registerHandler("/api/auth/register", &AuthController::registerUser);
app.registerHandler("/api/auth/login", &AuthController::login);

// 帖子路由
app.registerHandler("/api/posts", &PostController::getPosts, Get);
app.registerHandler("/api/posts", &PostController::createPost, Post);

// AI路由
app.registerHandler("/api/ai/chat", &AiController::chat, Post);
app.registerHandler("/api/ai/conversations", &AiController::getConversations, Get);
```

#### 控制器模式

```cpp
class PostController {
public:
    static void getPosts(const HttpRequestPtr& req, 
                        std::function<void(const HttpResponsePtr&)>&& callback) {
        // 1. 解析参数
        auto page = req->getParameter("page");
        auto limit = req->getParameter("limit");
        
        // 2. 调用服务层
        auto posts = PostService::getPosts(page, limit);
        
        // 3. 返回响应
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(Json::Value(posts).toStyledString());
        callback(resp);
    }
};
```

#### 服务层

```cpp
class PostService {
public:
    static vector<Post> getPosts(int page, int limit) {
        // 数据库查询
        auto sql = fmt::format(
            "SELECT * FROM posts ORDER BY created_at DESC LIMIT {} OFFSET {}",
            limit, (page - 1) * limit
        );
        
        // 执行查询并返回结果
        return db.query(sql);
    }
    
    static Post createPost(int userId, const string& content) {
        // 业务逻辑
        // 数据验证
        // 数据库插入
        // 返回创建的帖子
    }
};
```

### 数据库访问

使用Drogon ORM或原始SQL：

```cpp
// 示例：使用Drogon ORM
auto posts = drogon::orm::Mapper<Post>(dbClient)
    .orderBy(drogon::orm::SortOrder::DESC, "created_at")
    .limit(limit)
    .offset(offset)
    .findAll();
```

---

## AI服务集成

### 架构

```
Frontend → Backend API → llama-server → Qwen2.5 Model
                ↑              ↓
           Conversation DB   Response
```

### 工作流程

1. 用户发送消息到 `/api/ai/chat`
2. 后端保存消息到数据库
3. 后端转发请求到 llama-server
4. llama-server 使用Qwen2.5生成回复
5. 后端接收回复并保存到数据库
6. 返回给用户

### llama-server启动

```bash
cd echo-ai/llama.cpp/build/bin
./llama-server \
  -m ../../models/qwen2.5-1.5b/qwen2.5-1.5b-instruct-q4_k_m.gguf \
  -c 2048 \
  -t 4 \
  --port 18080 \
  --host 127.0.0.1
```

参数说明：
- `-m`: 模型文件路径
- `-c`: 上下文长度
- `-t`: 线程数
- `--port`: 监听端口

### API兼容

llama-server提供OpenAI兼容的API：

```bash
curl http://127.0.0.1:18080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "user", "content": "你好"}
    ]
  }'
```

---

## 部署指南

### 开发环境

参见README.md的快速开始部分。

### 生产环境

#### 1. 前端部署

```bash
cd echo-client
npm run build
# 将 dist/ 目录部署到Nginx或其他Web服务器
```

Nginx配置示例：
```nginx
server {
    listen 80;
    server_name moment.cumt.edu.cn;
    
    root /var/www/moment/dist;
    index index.html;
    
    location / {
        try_files $uri $uri/ /index.html;
    }
    
    location /api {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

#### 2. 后端部署

```bash
cd echo-server/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 使用systemd管理服务
sudo cp echo-server.service /etc/systemd/system/
sudo systemctl enable echo-server
sudo systemctl start echo-server
```

systemd服务文件 (`echo-server.service`):
```ini
[Unit]
Description=Moment Backend Service
After=network.target postgresql.service

[Service]
Type=simple
User=moment
WorkingDirectory=/opt/moment/echo-server/build
ExecStart=/opt/moment/echo-server/build/echo_server
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

#### 3. AI服务部署

```bash
# 创建systemd服务
sudo cp ai-server.service /etc/systemd/system/
sudo systemctl enable ai-server
sudo systemctl start ai-server
```

#### 4. 数据库配置

```
-- 创建数据库和用户
CREATE DATABASE moment_db;
CREATE USER moment_user WITH PASSWORD 'secure_password';
GRANT ALL PRIVILEGES ON DATABASE moment_db TO moment_user;

-- 运行迁移脚本
\i /path/to/schema.sql
```

### 环境变量

创建 `.env` 文件（不纳入版本控制）：

```bash
# 数据库
DB_HOST=localhost
DB_PORT=5432
DB_NAME=moment_db
DB_USER=moment_user
DB_PASSWORD=secure_password

# Redis（可选）
REDIS_HOST=localhost
REDIS_PORT=6379

# AI服务
AI_SERVER_URL=http://127.0.0.1:18080

# JWT密钥（生产环境）
JWT_SECRET=your-secret-key-here
```

---

## 性能优化

### 前端优化

1. **代码分割**: Vite自动处理
2. **图片优化**: 使用WebP格式，懒加载
3. **缓存策略**: Service Worker（待实现）

### 后端优化

1. **数据库索引**: 已添加常用查询索引
2. **连接池**: Drogon默认启用
3. **缓存**: Redis缓存热点数据（待实现）
4. **分页**: 所有列表接口支持分页

### AI优化

1. **模型量化**: 使用Q4_K_M量化，减小体积
2. **上下文限制**: 设置合理的context length
3. **并发控制**: 限制同时处理的请求数

---

## 监控和日志

### 日志级别

```
LOG_DEBUG << "调试信息";
LOG_INFO << "一般信息";
LOG_WARN << "警告";
LOG_ERROR << "错误";
```

### 日志文件

```
# 配置日志输出到文件
tail -f /var/log/moment/server.log
```

---

## 安全考虑

### 当前实现

- ✅ 密码哈希存储
- ✅ SQL注入防护（使用参数化查询）
- ✅ XSS防护（前端转义HTML）

### 待改进

- ⏳ JWT Token替换demo token
- ⏳ Rate Limiting防止滥用
- ⏳ CORS配置
- ⏳ HTTPS强制
- ⏳ 输入验证增强

---

## 测试

### 单元测试（待实现）

```cpp
// 使用Google Test
TEST(PostServiceTest, CreatePost) {
    auto post = PostService::createPost(1, "Test content");
    EXPECT_EQ(post.content, "Test content");
    EXPECT_GT(post.id, 0);
}
```

### API测试

```bash
cd echo-server
bash test_api.sh
```

---

## 常见问题

### Q: 如何添加新的API端点？

A: 
1. 在对应的Controller中添加方法
2. 在main.cpp中注册路由
3. 实现业务逻辑
4. 更新API文档

### Q: 如何修改数据库schema？

A:
1. 编写SQL迁移脚本
2. 在docs/目录下记录变更
3. 更新相关代码
4. 测试兼容性

### Q: 前端如何调试？

A:
1. 浏览器DevTools
2. Vite HMR热更新
3. Console日志
4. Network面板查看API请求

---

## 参考资料

- [Drogon官方文档](https://drogon.org/)
- [llama.cpp GitHub](https://github.com/ggerganov/llama.cpp)
- [Vite官方文档](https://vitejs.dev/)
- [PostgreSQL文档](https://www.postgresql.org/docs/)

---

有任何技术问题，欢迎提交Issue或联系维护团队！
