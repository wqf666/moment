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

    if (!row["media_url"].isNull()) {
        item["media_url"] = row["media_url"].as<std::string>();
    } else {
        item["media_url"] = "";
    }

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
            std::string mediaUrl = (*json).get("media_url", "").asString();

            if (content.empty() && mediaUrl.empty()) {
                callback(jsonResp(12002, "content or media_url is required"));
                return;
            }

            try {
                auto result = appctx::db->execSqlSync(
                    "INSERT INTO posts(user_id, content, media_url) VALUES(?, ?, ?)",
                    userId,
                    content,
                    mediaUrl
                );

                uint64_t postId = result.insertId();

                Json::Value data;
                data["id"] = static_cast<Json::Int64>(postId);
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["content"] = content;
                data["media_url"] = mediaUrl;

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
                    "SELECT p.id, p.user_id, p.content, p.media_url, "
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
                auto rows = appctx::db->execSqlSync(
                    "SELECT p.id, p.user_id, p.content, p.media_url, "
                    "p.created_at, p.updated_at, u.username "
                    "FROM posts p "
                    "LEFT JOIN users u ON p.user_id = u.id "
                    "WHERE p.id = ?",
                    postId
                );

                if (rows.empty()) {
                    callback(jsonResp(12004, "Post not found"));
                    return;
                }

                callback(jsonResp(0, "success", rowToPostJson(rows[0])));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get post db error: " << e.base().what() << std::endl;
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
            std::string mediaUrl = (*json).get("media_url", "").asString();

            if (content.empty() && mediaUrl.empty()) {
                callback(jsonResp(12002, "content or media_url is required"));
                return;
            }

            try {
                auto result = appctx::db->execSqlSync(
                    "UPDATE posts SET content = ?, media_url = ? WHERE id = ? AND user_id = ?",
                    content,
                    mediaUrl,
                    postId,
                    userId
                );

                Json::Value data;
                data["id"] = static_cast<Json::Int64>(postId);
                data["content"] = content;
                data["media_url"] = mediaUrl;

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
}

} // namespace controllers