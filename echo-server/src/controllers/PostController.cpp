#include "controllers/PostController.h"

#include <iostream>
#include <json/json.h>
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include "common/AppContext.h"
#include "common/Auth.h"
#include "common/Parse.h"
#include "common/Response.h"

namespace controllers {

namespace {

drogon::HttpResponsePtr jsonResp(
    int code,
    const std::string& message,
    const Json::Value& data = Json::Value(Json::objectValue)
) {
    return makeJsonResponse(code, message, data);
}

Json::Value rowToPostJson(const drogon::orm::Row& row) {
    Json::Value item;

    item["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
    item["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());

    if (!row["username"].isNull()) {
        item["username"] = row["username"].as<std::string>();
    }

    if (!row["content"].isNull()) {
        item["content"] = row["content"].as<std::string>();
    }

    // 统一使用 image_url，同时返回 media_url 以兼容旧前端
    std::string imageUrl = "";
    if (!row["media_url"].isNull()) {
        imageUrl = row["media_url"].as<std::string>();
    }
    
    item["image_url"] = imageUrl;
    item["media_url"] = imageUrl; // 兼容旧字段，后续版本可删除

    if (!row["created_at"].isNull()) {
        item["created_at"] = row["created_at"].as<std::string>();
    }

    if (!row["updated_at"].isNull()) {
        item["updated_at"] = row["updated_at"].as<std::string>();
    }

    return item;
}

} // namespace

void registerPostRoutes() {
    using namespace drogon;

    app().registerHandler(
        "/api/posts/create",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(12000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(12001, "Request body must be JSON"));
                return;
            }

            std::string content = (*json).get("content", "").asString();
            // 优先使用 image_url，兼容旧的 media_url
            std::string imageUrl = (*json).get("image_url", "").asString();
            if (imageUrl.empty()) {
                imageUrl = (*json).get("media_url", "").asString();
            }

            if (content.empty() && imageUrl.empty()) {
                callback(jsonResp(12002, "content or image_url is required"));
                return;
            }

            try {
                auto result = appctx::db->execSqlSync(
                    "INSERT INTO posts(user_id, content, image_url) VALUES(?, ?, ?)",
                    userId,
                    content,
                    imageUrl
                );

                uint64_t postId = result.insertId();

                Json::Value data;
                data["id"] = static_cast<Json::Int64>(postId);
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["content"] = content;
                data["image_url"] = imageUrl;
                data["media_url"] = imageUrl; // 兼容旧字段

                callback(jsonResp(0, "Post created", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "create post db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/posts",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int page = 1;
            int pageSize = 20;

            try {
                auto pageParam = req->getParameter("page");
                auto pageSizeParam = req->getParameter("page_size");

                if (!pageParam.empty()) {
                    page = std::max(1, std::stoi(pageParam));
                }

                if (!pageSizeParam.empty()) {
                    pageSize = std::min(50, std::max(1, std::stoi(pageSizeParam)));
                }
            } catch (...) {
                page = 1;
                pageSize = 20;
            }

            int offset = (page - 1) * pageSize;

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT p.id, p.user_id, p.content, p.image_url AS media_url, "
                    "p.created_at, p.updated_at, u.username "
                    "FROM posts p "
                    "LEFT JOIN users u ON p.user_id = u.id "
                    "ORDER BY p.created_at DESC "
                    "LIMIT ? OFFSET ?",
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    list.append(rowToPostJson(row));
                }

                Json::Value data;
                data["page"] = page;
                data["page_size"] = pageSize;
                data["list"] = list;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "list posts db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/posts/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& postIdText) {
            (void)req;

            int64_t postId = 0;

            if (!parsePositiveInt64(postIdText, postId)) {
                callback(jsonResp(12003, "Invalid post id"));
                return;
            }

            try {
                // 获取帖子详情
                auto postRows = appctx::db->execSqlSync(
                    "SELECT p.id, p.user_id, p.content, p.image_url AS media_url, "
                    "p.created_at, p.updated_at, u.username, u.avatar_url, "
                    "COUNT(DISTINCT pl.id) AS like_count, "
                    "COUNT(DISTINCT c.id) AS comment_count "
                    "FROM posts p "
                    "LEFT JOIN users u ON p.user_id = u.id "
                    "LEFT JOIN post_likes pl ON p.id = pl.post_id "
                    "LEFT JOIN comments c ON p.id = c.post_id "
                    "WHERE p.id = ? "
                    "GROUP BY p.id",
                    postId
                );

                if (postRows.empty()) {
                    callback(jsonResp(12004, "Post not found"));
                    return;
                }

                const auto& postRow = postRows[0];
                Json::Value post = rowToPostJson(postRow);
                post["like_count"] = postRow["like_count"].as<int>();
                post["comment_count"] = postRow["comment_count"].as<int>();
                post["avatar_url"] = postRow["avatar_url"].isNull() ? "" : postRow["avatar_url"].as<std::string>();

                // 获取评论列表（带用户信息）
                auto commentRows = appctx::db->execSqlSync(
                    "SELECT c.id, c.post_id, c.user_id, c.content, c.created_at, "
                    "u.username, u.avatar_url "
                    "FROM comments c "
                    "LEFT JOIN users u ON c.user_id = u.id "
                    "WHERE c.post_id = ? "
                    "ORDER BY c.created_at ASC",
                    postId
                );

                Json::Value comments(Json::arrayValue);
                for (const auto& row : commentRows) {
                    Json::Value comment;
                    comment["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                    comment["post_id"] = static_cast<Json::Int64>(row["post_id"].as<int64_t>());
                    comment["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                    comment["username"] = row["username"].isNull() ? "" : row["username"].as<std::string>();
                    comment["avatar_url"] = row["avatar_url"].isNull() ? "" : row["avatar_url"].as<std::string>();
                    comment["content"] = row["content"].as<std::string>();
                    comment["created_at"] = row["created_at"].isNull() ? "" : row["created_at"].as<std::string>();
                    comments.append(comment);
                }

                Json::Value data;
                data["post"] = post;
                data["comments"] = comments;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get post detail db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/posts/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& postIdText) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(12000, errorMessage));
                return;
            }

            int64_t postId = 0;

            if (!parsePositiveInt64(postIdText, postId)) {
                callback(jsonResp(12003, "Invalid post id"));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(12001, "Request body must be JSON"));
                return;
            }

            std::string content = (*json).get("content", "").asString();
            // 优先使用 image_url，兼容旧的 media_url
            std::string imageUrl = (*json).get("image_url", "").asString();
            if (imageUrl.empty()) {
                imageUrl = (*json).get("media_url", "").asString();
            }

            if (content.empty() && imageUrl.empty()) {
                callback(jsonResp(12002, "content or image_url is required"));
                return;
            }

            try {
                auto result = appctx::db->execSqlSync(
                    "UPDATE posts SET content = ?, image_url = ? WHERE id = ? AND user_id = ?",
                    content,
                    imageUrl,
                    postId,
                    userId
                );

                Json::Value data;
                data["id"] = static_cast<Json::Int64>(postId);
                data["content"] = content;
                data["image_url"] = imageUrl;
                data["media_url"] = imageUrl; // 兼容旧字段

                callback(jsonResp(0, "Post updated", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "update post db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Put}
    );

    app().registerHandler(
        "/api/posts/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& postIdText) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(12000, errorMessage));
                return;
            }

            int64_t postId = 0;

            if (!parsePositiveInt64(postIdText, postId)) {
                callback(jsonResp(12003, "Invalid post id"));
                return;
            }

            try {
                appctx::db->execSqlSync(
                    "DELETE FROM posts WHERE id = ? AND user_id = ?",
                    postId,
                    userId
                );

                Json::Value data;
                data["id"] = static_cast<Json::Int64>(postId);

                callback(jsonResp(0, "Post deleted", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "delete post db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Delete}
    );

    // 切换点赞状态
    app().registerHandler(
        "/api/posts/like/toggle",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(12000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(12001, "Request body must be JSON"));
                return;
            }

            int64_t postId = 0;

            if (!parseJsonInt64(*json, "post_id", postId)) {
                callback(jsonResp(12005, "Invalid post_id"));
                return;
            }

            try {
                // 检查帖子是否存在
                auto postRows = appctx::db->execSqlSync(
                    "SELECT id FROM posts WHERE id = ?",
                    postId
                );

                if (postRows.empty()) {
                    callback(jsonResp(12004, "Post not found"));
                    return;
                }

                // 检查是否已点赞
                auto likeRows = appctx::db->execSqlSync(
                    "SELECT id FROM post_likes WHERE post_id = ? AND user_id = ?",
                    postId,
                    userId
                );

                bool isLiked = !likeRows.empty();

                if (isLiked) {
                    // 取消点赞
                    appctx::db->execSqlSync(
                        "DELETE FROM post_likes WHERE post_id = ? AND user_id = ?",
                        postId,
                        userId
                    );

                    Json::Value data;
                    data["liked"] = false;
                    callback(jsonResp(0, "Unliked", data));
                } else {
                    // 添加点赞
                    appctx::db->execSqlSync(
                        "INSERT INTO post_likes(post_id, user_id) VALUES(?, ?)",
                        postId,
                        userId
                    );

                    Json::Value data;
                    data["liked"] = true;
                    callback(jsonResp(0, "Liked", data));
                }
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "toggle like db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Post}
    );

    // 发表评论
    app().registerHandler(
        "/api/posts/comment",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(12000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(12001, "Request body must be JSON"));
                return;
            }

            int64_t postId = 0;

            if (!parseJsonInt64(*json, "post_id", postId)) {
                callback(jsonResp(12005, "Invalid post_id"));
                return;
            }

            std::string content = (*json).get("content", "").asString();

            if (content.empty()) {
                callback(jsonResp(12006, "Comment content is required"));
                return;
            }

            try {
                // 检查帖子是否存在
                auto postRows = appctx::db->execSqlSync(
                    "SELECT id FROM posts WHERE id = ?",
                    postId
                );

                if (postRows.empty()) {
                    callback(jsonResp(12004, "Post not found"));
                    return;
                }

                auto result = appctx::db->execSqlSync(
                    "INSERT INTO comments(post_id, user_id, content) VALUES(?, ?, ?)",
                    postId,
                    userId,
                    content
                );

                uint64_t commentId = result.insertId();

                Json::Value data;
                data["comment_id"] = static_cast<Json::Int64>(commentId);
                data["post_id"] = static_cast<Json::Int64>(postId);
                data["content"] = content;

                callback(jsonResp(0, "Comment created", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "create comment db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Post}
    );

    // 最新帖子列表（带统计信息）
    app().registerHandler(
        "/api/posts/latest",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int page = 1;
            int pageSize = 20;

            try {
                auto pageParam = req->getParameter("page");
                auto pageSizeParam = req->getParameter("page_size");

                if (!pageParam.empty()) {
                    page = std::max(1, std::stoi(pageParam));
                }

                if (!pageSizeParam.empty()) {
                    pageSize = std::min(50, std::max(1, std::stoi(pageSizeParam)));
                }
            } catch (...) {
                page = 1;
                pageSize = 20;
            }

            int offset = (page - 1) * pageSize;

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT p.id, p.user_id, p.content, p.image_url AS media_url, "
                    "p.created_at, p.updated_at, u.username, "
                    "COUNT(DISTINCT pl.id) AS like_count, "
                    "COUNT(DISTINCT c.id) AS comment_count "
                    "FROM posts p "
                    "LEFT JOIN users u ON p.user_id = u.id "
                    "LEFT JOIN post_likes pl ON p.id = pl.post_id "
                    "LEFT JOIN comments c ON p.id = c.post_id "
                    "GROUP BY p.id "
                    "ORDER BY p.created_at DESC "
                    "LIMIT ? OFFSET ?",
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    Json::Value item = rowToPostJson(row);
                    item["like_count"] = row["like_count"].as<int>();
                    item["comment_count"] = row["comment_count"].as<int>();
                    item["is_owner"] = false; // 需要登录后判断
                    item["liked"] = false; // 需要登录后判断
                    list.append(item);
                }

                Json::Value data;
                data["page"] = page;
                data["page_size"] = pageSize;
                data["list"] = list;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "list latest posts db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    // 关注流
    app().registerHandler(
        "/api/feed/following",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(12000, errorMessage));
                return;
            }

            int page = 1;
            int pageSize = 20;

            try {
                auto pageParam = req->getParameter("page");
                auto pageSizeParam = req->getParameter("page_size");

                if (!pageParam.empty()) {
                    page = std::max(1, std::stoi(pageParam));
                }

                if (!pageSizeParam.empty()) {
                    pageSize = std::min(50, std::max(1, std::stoi(pageSizeParam)));
                }
            } catch (...) {
                page = 1;
                pageSize = 20;
            }

            int offset = (page - 1) * pageSize;

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT p.id, p.user_id, p.content, p.image_url AS media_url, "
                    "p.created_at, p.updated_at, u.username, "
                    "COUNT(DISTINCT pl.id) AS like_count, "
                    "COUNT(DISTINCT c.id) AS comment_count "
                    "FROM posts p "
                    "JOIN users u ON p.user_id = u.id "
                    "JOIN user_follows uf ON p.user_id = uf.following_id "
                    "LEFT JOIN post_likes pl ON p.id = pl.post_id "
                    "LEFT JOIN comments c ON p.id = c.post_id "
                    "WHERE uf.follower_id = ? "
                    "GROUP BY p.id "
                    "ORDER BY p.created_at DESC "
                    "LIMIT ? OFFSET ?",
                    userId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    Json::Value item = rowToPostJson(row);
                    item["like_count"] = row["like_count"].as<int>();
                    item["comment_count"] = row["comment_count"].as<int>();
                    item["is_owner"] = false;
                    item["liked"] = false;
                    list.append(item);
                }

                Json::Value data;
                data["page"] = page;
                data["page_size"] = pageSize;
                data["list"] = list;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "list following feed db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );
}

} // namespace controllers