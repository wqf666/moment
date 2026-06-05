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