#include "controllers/FollowController.h"

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

} // namespace

void registerFollowRoutes() {
    using namespace drogon;

    // 切换关注状态
    app().registerHandler(
        "/api/follows/toggle",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(16000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(16001, "Request body must be JSON"));
                return;
            }

            int64_t targetUserId = 0;

            if (!parseJsonInt64(*json, "target_user_id", targetUserId)) {
                callback(jsonResp(16002, "Invalid target_user_id"));
                return;
            }

            if (targetUserId == userId) {
                callback(jsonResp(16003, "Cannot follow yourself"));
                return;
            }

            try {
                // 检查目标用户是否存在
                auto userRows = appctx::db->execSqlSync(
                    "SELECT id FROM users WHERE id = ?",
                    targetUserId
                );

                if (userRows.empty()) {
                    callback(jsonResp(16004, "Target user not found"));
                    return;
                }

                // 检查是否已经关注
                auto followRows = appctx::db->execSqlSync(
                    "SELECT id FROM user_follows WHERE follower_id = ? AND following_id = ?",
                    userId,
                    targetUserId
                );

                bool isFollowing = !followRows.empty();

                if (isFollowing) {
                    // 取消关注
                    appctx::db->execSqlSync(
                        "DELETE FROM user_follows WHERE follower_id = ? AND following_id = ?",
                        userId,
                        targetUserId
                    );

                    Json::Value data;
                    data["is_following"] = false;
                    callback(jsonResp(0, "Unfollowed", data));
                } else {
                    // 添加关注
                    appctx::db->execSqlSync(
                        "INSERT INTO user_follows(follower_id, following_id) VALUES(?, ?)",
                        userId,
                        targetUserId
                    );

                    Json::Value data;
                    data["is_following"] = true;
                    callback(jsonResp(0, "Followed", data));
                }
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "toggle follow db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Post}
    );

    // 获取粉丝列表
    app().registerHandler(
        "/api/follows/followers",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(16000, errorMessage));
                return;
            }

            // 支持查询指定用户的粉丝，默认查询当前用户
            int64_t targetUserId = userId;
            auto targetUserParam = req->getParameter("user_id");
            if (!targetUserParam.empty()) {
                if (!parsePositiveInt64(targetUserParam, targetUserId)) {
                    callback(jsonResp(16002, "Invalid user_id"));
                    return;
                }
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
                    "SELECT u.id, u.username, u.nickname, u.avatar_url, uf.created_at "
                    "FROM user_follows uf "
                    "JOIN users u ON uf.follower_id = u.id "
                    "WHERE uf.following_id = ? "
                    "ORDER BY uf.created_at DESC "
                    "LIMIT ? OFFSET ?",
                    targetUserId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
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

                    if (!row["created_at"].isNull()) {
                        item["followed_at"] = row["created_at"].as<std::string>();
                    }

                    list.append(item);
                }

                Json::Value data;
                data["page"] = page;
                data["page_size"] = pageSize;
                data["followers"] = list; // 统一使用 followers 字段

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get followers db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    // 获取关注列表
    app().registerHandler(
        "/api/follows/following",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(16000, errorMessage));
                return;
            }

            // 支持查询指定用户的关注，默认查询当前用户
            int64_t targetUserId = userId;
            auto targetUserParam = req->getParameter("user_id");
            if (!targetUserParam.empty()) {
                if (!parsePositiveInt64(targetUserParam, targetUserId)) {
                    callback(jsonResp(16002, "Invalid user_id"));
                    return;
                }
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
                    "SELECT u.id, u.username, u.nickname, u.avatar_url, uf.created_at "
                    "FROM user_follows uf "
                    "JOIN users u ON uf.following_id = u.id "
                    "WHERE uf.follower_id = ? "
                    "ORDER BY uf.created_at DESC "
                    "LIMIT ? OFFSET ?",
                    targetUserId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
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

                    if (!row["created_at"].isNull()) {
                        item["followed_at"] = row["created_at"].as<std::string>();
                    }

                    list.append(item);
                }

                Json::Value data;
                data["page"] = page;
                data["page_size"] = pageSize;
                data["following"] = list; // 统一使用 following 字段

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "get following db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );
}

} // namespace controllers