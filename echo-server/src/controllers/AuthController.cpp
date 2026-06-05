#include "controllers/AuthController.h"

#include <iostream>
#include <json/json.h>
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include "common/AppContext.h"
#include "common/Response.h"

namespace controllers {

void registerAuthRoutes() {
    using namespace drogon;

    app().registerHandler(
        "/api/auth/register",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            if (!json) {
                callback(makeJsonResponse(1001, "Request body must be JSON"));
                return;
            }

            std::string username = (*json).get("username", "").asString();
            std::string password = (*json).get("password", "").asString();

            if (username.empty() || password.empty()) {
                callback(makeJsonResponse(1002, "username and password are required"));
                return;
            }

            if (username.length() > 32) {
                callback(makeJsonResponse(1003, "username must not exceed 32 characters"));
                return;
            }

            if (password.length() < 6) {
                callback(makeJsonResponse(1004, "password must be at least 6 characters"));
                return;
            }

            try {
                auto exists = appctx::db->execSqlSync(
                    "SELECT id FROM users WHERE username = ?",
                    username
                );

                if (!exists.empty()) {
                    callback(makeJsonResponse(1005, "username already exists"));
                    return;
                }

                auto result = appctx::db->execSqlSync(
                    "INSERT INTO users(username, password_hash) VALUES(?, SHA2(?, 256))",
                    username,
                    password
                );

                uint64_t userId = result.insertId();

                if (userId == 0) {
                    callback(makeJsonResponse(5002, "Register failed"));
                    return;
                }

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["username"] = username;
                data["token"] = "demo_token_" + std::to_string(userId);

                callback(makeJsonResponse(0, "Register succeeded", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/auth/login",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            if (!json) {
                callback(makeJsonResponse(2001, "Request body must be JSON"));
                return;
            }

            std::string username = (*json).get("username", "").asString();
            std::string password = (*json).get("password", "").asString();

            if (username.empty() || password.empty()) {
                callback(makeJsonResponse(2002, "username and password are required"));
                return;
            }

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT id, username FROM users WHERE username = ? AND password_hash = SHA2(?, 256)",
                    username,
                    password
                );

                if (rows.empty()) {
                    callback(makeJsonResponse(2004, "Invalid username or password"));
                    return;
                }

                int64_t userId = rows[0]["id"].as<int64_t>();

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["username"] = rows[0]["username"].as<std::string>();
                data["token"] = "demo_token_" + std::to_string(userId);

                callback(makeJsonResponse(0, "Login succeeded", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );
}

}
