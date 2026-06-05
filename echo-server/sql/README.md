# 数据库使用指南

## 📋 概述

Moment项目统一使用**MySQL 8.0+**作为数据库，所有表结构定义已整合到 [`sql/init.sql`](sql/init.sql)。

---

## 🚀 快速开始

### 1. 安装MySQL

```bash
# Ubuntu/Debian
sudo apt-get install mysql-server

# CentOS/RHEL
sudo yum install mysql-server

# macOS (Homebrew)
brew install mysql
```

### 2. 创建数据库用户

```sql
-- 以root身份登录MySQL
mysql -u root -p

-- 创建数据库和用户
CREATE DATABASE IF NOT EXISTS echo_app 
    DEFAULT CHARACTER SET utf8mb4 
    DEFAULT COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'echo_user'@'localhost' IDENTIFIED BY '123456';
GRANT ALL PRIVILEGES ON echo_app.* TO 'echo_user'@'localhost';
FLUSH PRIVILEGES;
```

### 3. 执行初始化脚本

```bash
cd /home/wqf/workspace/echo-server
mysql -u echo_user -p < sql/init.sql
```

输入密码 `123456` 后，脚本将自动创建所有7个数据表。

### 4. 验证安装

```bash
mysql -u echo_user -p echo_app -e "SHOW TABLES;"
```

应该看到以下表格：
- users
- posts
- comments
- user_follows
- post_likes
- ai_conversations
- ai_messages

---

## 📊 数据库表结构

### 核心表

#### 1. users (用户表)
存储用户基本信息和个人资料。

**关键字段**:
- `id`: 主键，自增
- `username`: 用户名（唯一）
- `password_hash`: SHA256加密的密码
- `nickname`: 昵称（可选）
- `avatar_url`: 头像URL
- `bio`: 个人简介
- `cover_image_url`: 主页封面图

#### 2. posts (帖子表)
存储用户发布的动态内容。

**关键字段**:
- `id`: 主键，自增
- `user_id`: 作者ID（外键）
- `content`: 帖子内容（最多2000字符）
- `image_url`: 图片URL（可选）
- `like_count`: 点赞数（冗余字段，便于查询）
- `comment_count`: 评论数（冗余字段）

**索引**:
- `idx_posts_user_id`: 按用户查询帖子
- `idx_posts_created_at`: 按时间排序
- `idx_posts_user_id_id`: 复合索引优化个人主页查询
- `idx_posts_user_image`: 优化图片墙查询

#### 3. comments (评论表)
存储对帖子的评论。

**关键字段**:
- `id`: 主键，自增
- `post_id`: 所属帖子（外键）
- `user_id`: 评论者（外键）
- `content`: 评论内容（最多1000字符）

#### 4. user_follows (关注关系表)
记录用户之间的关注关系（多对多）。

**关键字段**:
- `follower_id`: 关注者
- `following_id`: 被关注者
- **唯一约束**: `(follower_id, following_id)` 防止重复关注

#### 5. post_likes (点赞表)
记录用户对帖子的点赞行为。

**关键字段**:
- `post_id`: 被点赞的帖子
- `user_id`: 点赞用户
- **唯一约束**: `(post_id, user_id)` 防止重复点赞

### AI相关表

#### 6. ai_conversations (AI对话表)
管理用户与AI的会话。

**关键字段**:
- `id`: 对话ID
- `user_id`: 用户ID（外键）
- `title`: 对话标题（默认"AI Chat"）

#### 7. ai_messages (AI消息表)
存储对话中的每条消息。

**关键字段**:
- `conversation_id`: 所属对话（外键）
- `user_id`: 用户ID
- `role`: 角色（'user' 或 'assistant'）
- `content`: 消息内容

---

## 🔧 常用SQL操作

### 查询示例

```sql
-- 获取最新帖子（带作者信息和点赞状态）
SELECT p.id, p.user_id, u.username, 
       COALESCE(u.nickname, '') AS nickname,
       COALESCE(u.avatar_url, '') AS avatar_url,
       p.content, COALESCE(p.image_url, '') AS image_url,
       DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at,
       COALESCE(lc.like_count, 0) AS like_count,
       CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked
FROM posts p
JOIN users u ON u.id = p.user_id
LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ?
LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id
ORDER BY p.id DESC LIMIT 20 OFFSET 0;

-- 获取用户的粉丝列表
SELECT u.id, u.username, 
       COALESCE(u.nickname, '') AS nickname,
       COALESCE(u.avatar_url, '') AS avatar_url,
       DATE_FORMAT(f.created_at, '%Y-%m-%d %H:%i:%s') AS followed_at
FROM user_follows f
JOIN users u ON u.id = f.follower_id
WHERE f.following_id = ?
ORDER BY f.id DESC LIMIT 20;

-- 获取关注流（关注的用户发布的帖子）
SELECT p.*, u.username, u.nickname, u.avatar_url
FROM posts p
JOIN users u ON u.id = p.user_id
INNER JOIN user_follows uf ON uf.following_id = p.user_id
WHERE uf.follower_id = ?
ORDER BY p.created_at DESC LIMIT 20;
```

### 插入示例

```sql
-- 注册用户（密码使用SHA256加密）
INSERT INTO users(username, password_hash) 
VALUES('testuser', SHA2('password123', 256));

-- 发布帖子
INSERT INTO posts(user_id, content, image_url) 
VALUES(1, '这是我的第一条动态！', NULL);

-- 添加评论
INSERT INTO comments(post_id, user_id, content) 
VALUES(1, 2, '写得真好！');

-- 关注用户
INSERT INTO user_follows(follower_id, following_id) 
VALUES(1, 2);

-- 点赞帖子
INSERT INTO post_likes(post_id, user_id) 
VALUES(1, 2);
```

### 更新示例

```sql
-- 更新用户资料
UPDATE users 
SET nickname = '矿大小明', 
    avatar_url = '/uploads/avatar.jpg',
    bio = '计算机科学与技术专业'
WHERE id = 1;

-- 更新帖子内容
UPDATE posts 
SET content = '更新后的内容'
WHERE id = 1 AND user_id = 1;

-- 切换点赞状态
-- 如果已点赞则取消，未点赞则添加
DELETE FROM post_likes WHERE post_id = 1 AND user_id = 2;
-- 或
INSERT INTO post_likes(post_id, user_id) VALUES(1, 2);
```

### 删除示例

```sql
-- 删除帖子（级联删除评论和点赞）
DELETE FROM posts WHERE id = 1 AND user_id = 1;

-- 取消关注
DELETE FROM user_follows 
WHERE follower_id = 1 AND following_id = 2;

-- 删除评论
DELETE FROM comments WHERE id = 1 AND user_id = 2;
```

---

## 🎯 性能优化建议

### 1. 索引策略

已创建的索引覆盖了主要查询场景：
- 按用户查询（posts、comments）
- 按时间排序（posts、comments）
- 关注关系查询（user_follows）
- 点赞状态查询（post_likes）

### 2. 冗余字段

`posts`表中的`like_count`和`comment_count`是冗余字段，用于避免实时COUNT查询，提升性能。

**维护策略**:
```sql
-- 点赞时更新计数
UPDATE posts SET like_count = like_count + 1 WHERE id = ?;
UPDATE posts SET like_count = like_count - 1 WHERE id = ?;

-- 评论时更新计数
UPDATE posts SET comment_count = comment_count + 1 WHERE id = ?;
DELETE FROM comments WHERE id = ?;
UPDATE posts SET comment_count = comment_count - 1 WHERE id = ?;
```

### 3. 分页查询

始终使用LIMIT和OFFSET进行分页：
```sql
SELECT * FROM posts ORDER BY id DESC LIMIT 20 OFFSET 0;  -- 第1页
SELECT * FROM posts ORDER BY id DESC LIMIT 20 OFFSET 20; -- 第2页
```

### 4. 连接池配置

后端使用Drogon的连接池，建议在`main.cpp`中配置：
```cpp
g_db = drogon::orm::DbClient::newMysqlClient(
    "host=127.0.0.1 port=3306 dbname=echo_app user=echo_user password=123456",
    4  // 连接池大小
);
```

---

## 🔍 故障排查

### 常见问题

#### 1. 连接失败

**错误**: `Can't connect to MySQL server`

**解决**:
```bash
# 检查MySQL服务状态
sudo systemctl status mysql

# 启动MySQL服务
sudo systemctl start mysql

# 测试连接
mysql -h 127.0.0.1 -P 3306 -u echo_user -p echo_app
```

#### 2. 权限不足

**错误**: `Access denied for user 'echo_user'@'localhost'`

**解决**:
```sql
-- 以root身份重新授权
GRANT ALL PRIVILEGES ON echo_app.* TO 'echo_user'@'localhost';
FLUSH PRIVILEGES;
```

#### 3. 字符集问题

**错误**: 中文显示为乱码

**解决**:
```sql
-- 检查数据库字符集
SHOW CREATE DATABASE echo_app;

-- 应该是: DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
-- 如果不是，重建数据库
DROP DATABASE echo_app;
CREATE DATABASE echo_app DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

#### 4. 外键约束失败

**错误**: `Cannot add or update a child row: a foreign key constraint fails`

**原因**: 引用了不存在的记录

**解决**: 确保先插入父表记录，再插入子表记录

---

## 📝 备份与恢复

### 备份数据库

```bash
# 完整备份
mysqldump -u echo_user -p echo_app > backup_$(date +%Y%m%d).sql

# 仅备份结构
mysqldump -u echo_user -p --no-data echo_app > schema_backup.sql

# 仅备份数据
mysqldump -u echo_user -p --no-create-info echo_app > data_backup.sql
```

### 恢复数据库

```bash
mysql -u echo_user -p echo_app < backup_20260605.sql
```

---

## 🔐 安全建议

### 生产环境配置

1. **修改默认密码**
```sql
ALTER USER 'echo_user'@'localhost' IDENTIFIED BY '强密码';
```

2. **限制访问IP**
```sql
-- 只允许特定IP访问
CREATE USER 'echo_user'@'192.168.1.%' IDENTIFIED BY 'password';
```

3. **最小权限原则**
```sql
-- 应用只需要DML权限，不需要DDL
GRANT SELECT, INSERT, UPDATE, DELETE ON echo_app.* TO 'echo_user'@'localhost';
```

4. **启用SSL连接**
```cpp
// 在连接字符串中添加SSL选项
"host=127.0.0.1 ssl-mode=REQUIRED"
```

---

## 📚 参考资料

- [MySQL官方文档](https://dev.mysql.com/doc/)
- [Drogon ORM文档](https://github.com/drogonframework/drogon/wiki/ENG-Database)
- [MySQL性能优化最佳实践](https://dev.mysql.com/doc/refman/8.0/en/optimization.html)

---

## 🔄 版本历史

- **v1.0.0** (2026-06-05): 初始版本，统一使用MySQL，整合所有表结构到init.sql
