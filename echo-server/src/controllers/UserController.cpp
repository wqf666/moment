#include "controllers/UserController.h"

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

Json::Value rowToUserJson(const drogon::orm::Row& row) {
    Json::Value item;

    item["user_id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
    item["username"] = row["username"].as<std::string>();

    if (!row["nickname"].isNull()) {
        item["nickname"] = row["nickname"].as<std::string>();
    } else {
        item["nickname"] = "";
    }

    if (!row["avatar_url"].isNull()) {
        item["avatar_url"] = row["avatar_url"].as<std::string>();
    } else {
        item["avatar_url"] = "";
    }

    if (!row["bio"].isNull()) {
        item["bio"] = row["bio"].as<std::string>();
    } else {
        item["bio"] = "";
    }

    if (!row["cover_image_url"].isNull()) {
        item["cover_image_url"] = row["cover_image_url"].as<std::string>();
    } else {
        item["cover_image_url"] = "";
    }

    if (!row["created_at"].isNull()) {
        item["created_at"] = row["created_at"].as<std::string>();
    }

    return item;
}

} // namespace

void registerUserRoutes() {
    using namespace drogon;

    // 获取用户主页信息（包含用户资料、统计数据和帖子列表）
    app().registerHandler(
        "/api/users/home",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!parsePositiveInt64(req->getParameter("user_id"), userId)) {
                callback(jsonResp(15003, "Invalid user id"));
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
                // 获取用户信息
                auto userRows = appctx::db->execSqlSync(
                    "SELECT id, username, nickname, avatar_url, bio, cover_image_url, created_at "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (userRows.empty()) {
                    callback(jsonResp(15001, "User not found"));
                    return;
                }

                const auto& userRow = userRows[0];
                Json::Value user;
                user["user_id"] = static_cast<Json::Int64>(userRow["id"].as<int64_t>());
                user["username"] = userRow["username"].as<std::string>();
                user["nickname"] = userRow["nickname"].isNull() ? "" : userRow["nickname"].as<std::string>();
                user["avatar_url"] = userRow["avatar_url"].isNull() ? "" : userRow["avatar_url"].as<std::string>();
                user["bio"] = userRow["bio"].isNull() ? "" : userRow["bio"].as<std::string>();
                user["cover_image_url"] = userRow["cover_image_url"].isNull() ? "" : userRow["cover_image_url"].as<std::string>();
                user["created_at"] = userRow["created_at"].isNull() ? "" : userRow["created_at"].as<std::string>();

                // 获取统计数据
                auto statsRows = appctx::db->execSqlSync(
                    "SELECT "
                    "(SELECT COUNT(*) FROM posts WHERE user_id = ?) AS post_count, "
                    "(SELECT COUNT(*) FROM posts WHERE user_id = ? AND image_url != '') AS media_count, "
                    "(SELECT COUNT(*) FROM user_follows WHERE following_id = ?) AS follower_count, "
                    "(SELECT COUNT(*) FROM user_follows WHERE follower_id = ?) AS following_count",
                    userId, userId, userId, userId
                );

                Json::Value stats;
                stats["post_count"] = statsRows[0]["post_count"].as<int>();
                stats["media_count"] = statsRows[0]["media_count"].as<int>();
                stats["follower_count"] = statsRows[0]["follower_count"].as<int>();
                stats["following_count"] = statsRows[0]["following_count"].as<int>();

                // 获取用户帖子列表
                auto postRows = appctx::db->execSqlSync(
                    "SELECT p.id, p.user_id, p.content, p.image_url AS media_url, "
                    "p.created_at, p.updated_at, u.username "
                    "FROM posts p "
                    "LEFT JOIN users u ON p.user_id = u.id "
                    "WHERE p.user_id = ? "
                    "ORDER BY p.created_at DESC "
                    "LIMIT ? OFFSET ?",
                    userId, pageSize, offset
                );

                Json::Value posts(Json::arrayValue);
                for (const auto& row : postRows) {
                    Json::Value item;
                    item["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                    item["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                    item["username"] = row["username"].isNull() ? "" : row["username"].as<std::string>();
                    item["content"] = row["content"].isNull() ? "" : row["content"].as<std::string>();
                    item["media_url"] = row["media_url"].isNull() ? "" : row["media_url"].as<std::string>();
                    item["image_url"] = item["media_url"]; // 兼容旧字段
                    item["created_at"] = row["created_at"].isNull() ? "" : row["created_at"].as<std::string>();
                    item["updated_at"] = row["updated_at"].isNull() ? "" : row["updated_at"].as<std::string>();
                    posts.append(item);
                }

                Json::Value data;
                data["user"] = user;
                data["stats"] = stats;
                data["posts"] = posts;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get user home db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    // 获取当前用户信息
    app().registerHandler(
        "/api/users/me",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(15000, errorMessage));
                return;
            }

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT id, username, nickname, avatar_url, bio, cover_image_url, created_at "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (rows.empty()) {
                    callback(jsonResp(15001, "User not found"));
                    return;
                }

                callback(jsonResp(0, "success", rowToUserJson(rows[0])));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get user db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    // 更新用户资料
    app().registerHandler(
        "/api/users/profile/update",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(15000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(15002, "Request body must be JSON"));
                return;
            }

            std::string nickname = (*json).get("nickname", "").asString();
            std::string avatarUrl = (*json).get("avatar_url", "").asString();
            std::string bio = (*json).get("bio", "").asString();
            std::string coverImageUrl = (*json).get("cover_image_url", "").asString();

            try {
                appctx::db->execSqlSync(
                    "UPDATE users SET nickname = ?, avatar_url = ?, bio = ?, cover_image_url = ? WHERE id = ?",
                    nickname,
                    avatarUrl,
                    bio,
                    coverImageUrl,
                    userId
                );

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["nickname"] = nickname;
                data["avatar_url"] = avatarUrl;
                data["bio"] = bio;
                data["cover_image_url"] = coverImageUrl;

                callback(jsonResp(0, "Profile updated", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "update profile db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Post}
    );

    // 获取指定用户信息
    app().registerHandler(
        "/api/users/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& userIdText) {
            (void)req;

            int64_t userId = 0;

            if (!parsePositiveInt64(userIdText, userId)) {
                callback(jsonResp(15003, "Invalid user id"));
                return;
            }

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT id, username, nickname, avatar_url, bio, cover_image_url, created_at "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (rows.empty()) {
                    callback(jsonResp(15001, "User not found"));
                    return;
                }

                callback(jsonResp(0, "success", rowToUserJson(rows[0])));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get user db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );
}

} // namespace controllers