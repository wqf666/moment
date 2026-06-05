-- =====================================================
-- Moment - CUMT校园论坛 数据库初始化脚本
-- 数据库: MySQL 8.0+
-- 字符集: utf8mb4
-- =====================================================

-- 创建数据库（如果不存在）
CREATE DATABASE IF NOT EXISTS echo_app 
    DEFAULT CHARACTER SET utf8mb4 
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE echo_app;

-- =====================================================
-- 1. 用户表 (users)
-- =====================================================
CREATE TABLE IF NOT EXISTS users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '用户ID',
    username VARCHAR(50) UNIQUE NOT NULL COMMENT '用户名',
    password_hash VARCHAR(255) NOT NULL COMMENT '密码哈希(SHA256)',
    nickname VARCHAR(100) DEFAULT NULL COMMENT '昵称',
    avatar_url VARCHAR(500) DEFAULT NULL COMMENT '头像URL',
    bio VARCHAR(500) DEFAULT NULL COMMENT '个人简介',
    cover_image_url VARCHAR(500) DEFAULT NULL COMMENT '主页封面图URL',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    
    INDEX idx_users_username (username),
    INDEX idx_users_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户表';

-- =====================================================
-- 2. 帖子表 (posts)
-- =====================================================
CREATE TABLE IF NOT EXISTS posts (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '帖子ID',
    user_id BIGINT NOT NULL COMMENT '作者用户ID',
    content TEXT NOT NULL COMMENT '帖子内容',
    image_url VARCHAR(500) DEFAULT NULL COMMENT '图片URL',
    like_count BIGINT DEFAULT 0 COMMENT '点赞数',
    comment_count BIGINT DEFAULT 0 COMMENT '评论数',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    
    INDEX idx_posts_user_id (user_id),
    INDEX idx_posts_created_at (created_at DESC),
    INDEX idx_posts_user_id_id (user_id, id),
    INDEX idx_posts_user_image (user_id, image_url(191)),
    
    CONSTRAINT fk_posts_user_id FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='帖子表';

-- =====================================================
-- 3. 评论表 (comments)
-- =====================================================
CREATE TABLE IF NOT EXISTS comments (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '评论ID',
    post_id BIGINT NOT NULL COMMENT '所属帖子ID',
    user_id BIGINT NOT NULL COMMENT '评论者用户ID',
    content TEXT NOT NULL COMMENT '评论内容',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    
    INDEX idx_comments_post_id (post_id),
    INDEX idx_comments_user_id (user_id),
    INDEX idx_comments_created_at (created_at),
    
    CONSTRAINT fk_comments_post_id FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
    CONSTRAINT fk_comments_user_id FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='评论表';

-- =====================================================
-- 4. 用户关注关系表 (user_follows)
-- =====================================================
CREATE TABLE IF NOT EXISTS user_follows (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '关注关系ID',
    follower_id BIGINT NOT NULL COMMENT '关注者用户ID',
    following_id BIGINT NOT NULL COMMENT '被关注者用户ID',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '关注时间',
    
    UNIQUE KEY uk_follower_following (follower_id, following_id),
    INDEX idx_follower_id (follower_id),
    INDEX idx_following_id (following_id),
    
    CONSTRAINT fk_user_follows_follower FOREIGN KEY (follower_id) REFERENCES users(id) ON DELETE CASCADE,
    CONSTRAINT fk_user_follows_following FOREIGN KEY (following_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户关注关系表';

-- =====================================================
-- 5. 帖子点赞表 (post_likes)
-- =====================================================
CREATE TABLE IF NOT EXISTS post_likes (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '点赞记录ID',
    post_id BIGINT NOT NULL COMMENT '帖子ID',
    user_id BIGINT NOT NULL COMMENT '点赞用户ID',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '点赞时间',
    
    UNIQUE KEY uk_post_user (post_id, user_id),
    INDEX idx_post_likes_post_id (post_id),
    INDEX idx_post_likes_user_id (user_id),
    
    CONSTRAINT fk_post_likes_post_id FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,
    CONSTRAINT fk_post_likes_user_id FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='帖子点赞表';

-- =====================================================
-- 6. AI对话表 (ai_conversations)
-- =====================================================
CREATE TABLE IF NOT EXISTS ai_conversations (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '对话ID',
    user_id BIGINT NOT NULL COMMENT '用户ID',
    title VARCHAR(100) NOT NULL DEFAULT 'AI Chat' COMMENT '对话标题',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    
    INDEX idx_ai_conversations_user_id (user_id),
    INDEX idx_ai_conversations_updated_at (updated_at),
    
    CONSTRAINT fk_ai_conversations_user_id FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='AI对话表';

-- =====================================================
-- 7. AI消息表 (ai_messages)
-- =====================================================
CREATE TABLE IF NOT EXISTS ai_messages (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '消息ID',
    conversation_id BIGINT NOT NULL COMMENT '所属对话ID',
    user_id BIGINT NOT NULL COMMENT '用户ID',
    role VARCHAR(20) NOT NULL COMMENT '角色(user/assistant)',
    content TEXT NOT NULL COMMENT '消息内容',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    
    INDEX idx_ai_messages_conversation_id (conversation_id),
    INDEX idx_ai_messages_user_id (user_id),
    INDEX idx_ai_messages_created_at (created_at),
    
    CONSTRAINT fk_ai_messages_conversation_id FOREIGN KEY (conversation_id) REFERENCES ai_conversations(id) ON DELETE CASCADE,
    CONSTRAINT fk_ai_messages_user_id FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='AI消息表';

-- =====================================================
-- 初始化完成提示
-- =====================================================
SELECT '✅ Moment数据库初始化完成！' AS message;
SELECT '📊 已创建7个数据表：' AS info;
SELECT '  - users (用户表)' AS table_info
UNION ALL SELECT '  - posts (帖子表)'
UNION ALL SELECT '  - comments (评论表)'
UNION ALL SELECT '  - user_follows (关注关系表)'
UNION ALL SELECT '  - post_likes (点赞表)'
UNION ALL SELECT '  - ai_conversations (AI对话表)'
UNION ALL SELECT '  - ai_messages (AI消息表)';
