-- 社交关系 + 个人资料扩展迁移脚本
-- 注意：如果 users 表中某些字段已经存在，对应 ALTER TABLE 会报 Duplicate column，可跳过该条后继续执行。

ALTER TABLE users ADD COLUMN avatar_url VARCHAR(500) NULL DEFAULT NULL COMMENT '用户头像 URL';
ALTER TABLE users ADD COLUMN bio VARCHAR(500) NULL DEFAULT NULL COMMENT '个人简介';
ALTER TABLE users ADD COLUMN cover_image_url VARCHAR(500) NULL DEFAULT NULL COMMENT '个人主页头图 URL';

CREATE TABLE IF NOT EXISTS user_follows (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    follower_id BIGINT NOT NULL COMMENT '关注者用户ID',
    following_id BIGINT NOT NULL COMMENT '被关注者用户ID',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_follower_following (follower_id, following_id),
    KEY idx_follower_id (follower_id),
    KEY idx_following_id (following_id),
    CONSTRAINT fk_user_follows_follower
        FOREIGN KEY (follower_id) REFERENCES users(id)
        ON DELETE CASCADE,
    CONSTRAINT fk_user_follows_following
        FOREIGN KEY (following_id) REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户关注关系表';

-- 推荐补充索引，提升个人主页、关注流、图片主页查询速度。
CREATE INDEX idx_posts_user_id_id ON posts(user_id, id);
CREATE INDEX idx_posts_user_image ON posts(user_id, image_url(191));
