#include "controllers/AiController.h"

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

void registerAiRoutes() {
    using namespace drogon;

    app().registerHandler(
        "/api/ai/chat",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(13000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();

            if (!json) {
                callback(jsonResp(13001, "Request body must be JSON"));
                return;
            }

            std::string message = (*json).get("message", "").asString();

            if (message.empty()) {
                callback(jsonResp(13002, "message is required"));
                return;
            }

            int64_t conversationId = 0;

            if (!parseOptionalJsonInt64(*json, "conversation_id", conversationId)) {
                callback(jsonResp(13003, "Invalid conversation_id"));
                return;
            }

            try {
                if (conversationId <= 0) {
                    auto result = appctx::db->execSqlSync(
                        "INSERT INTO ai_conversations(user_id, title) VALUES(?, ?)",
                        userId,
                        message.substr(0, 30)
                    );

                    conversationId = static_cast<int64_t>(result.insertId());
                } else {
                    // 验证会话所有权
                    auto ownerRows = appctx::db->execSqlSync(
                        "SELECT id FROM ai_conversations WHERE id = ? AND user_id = ?",
                        conversationId,
                        userId
                    );

                    if (ownerRows.empty()) {
                        callback(jsonResp(13007, "Conversation not found"));
                        return;
                    }
                }

                appctx::db->execSqlSync(
                    "INSERT INTO ai_messages(conversation_id, role, content, user_id) VALUES(?, 'user', ?, ?)",
                    conversationId,
                    message,
                    userId
                );

                // 更新会话时间
                appctx::db->execSqlSync(
                    "UPDATE ai_conversations SET updated_at = NOW() WHERE id = ?",
                    conversationId
                );

                // 读取最近的历史消息（最多20条，避免上下文过长）
                auto historyRows = appctx::db->execSqlSync(
                    "SELECT role, content FROM ai_messages "
                    "WHERE conversation_id = ? "
                    "ORDER BY id DESC LIMIT 20",
                    conversationId
                );

                Json::Value aiReq;
                aiReq["model"] = "qwen2.5-1.5b-instruct";

                Json::Value messages(Json::arrayValue);

                // 系统提示词
                Json::Value systemMsg;
                systemMsg["role"] = "system";
                systemMsg["content"] = "你是一个校园论坛 AI 助手，请用简洁、友好、实用的方式回答用户问题。";
                messages.append(systemMsg);

                // 按时间顺序添加历史消息（需要反转）
                std::vector<Json::Value> historyMessages;
                for (const auto& row : historyRows) {
                    Json::Value msg;
                    msg["role"] = row["role"].as<std::string>();
                    msg["content"] = row["content"].as<std::string>();
                    historyMessages.push_back(msg);
                }

                // 反转消息顺序，使其按时间正序排列
                for (auto it = historyMessages.rbegin(); it != historyMessages.rend(); ++it) {
                    messages.append(*it);
                }

                // 添加当前用户消息
                Json::Value userMsg;
                userMsg["role"] = "user";
                userMsg["content"] = message;
                messages.append(userMsg);

                aiReq["messages"] = messages;
                aiReq["temperature"] = 0.7;
                aiReq["stream"] = false;

                auto aiHttpReq = HttpRequest::newHttpJsonRequest(aiReq);
                aiHttpReq->setMethod(Post);
                aiHttpReq->setPath("/v1/chat/completions");

                appctx::aiClient->sendRequest(
                    aiHttpReq,
                    [callback, conversationId, userId](ReqResult result, const HttpResponsePtr& aiResp) {
                        if (result != ReqResult::Ok || !aiResp) {
                            callback(jsonResp(13004, "AI service unavailable"));
                            return;
                        }

                        auto aiJson = aiResp->getJsonObject();

                        if (!aiJson) {
                            callback(jsonResp(13005, "Invalid AI response"));
                            return;
                        }

                        std::string reply;

                        try {
                            const auto& choices = (*aiJson)["choices"];

                            if (
                                choices.isArray() &&
                                choices.size() > 0 &&
                                choices[0].isMember("message") &&
                                choices[0]["message"].isMember("content")
                            ) {
                                reply = choices[0]["message"]["content"].asString();
                            }
                        } catch (...) {
                            reply.clear();
                        }

                        if (reply.empty()) {
                            reply = "抱歉，AI 暂时没有生成有效回复。";
                        }

                        try {
                            appctx::db->execSqlSync(
                                "INSERT INTO ai_messages(conversation_id, role, content, user_id) VALUES(?, 'assistant', ?, ?)",
                                conversationId,
                                reply,
                                userId
                            );
                        } catch (const drogon::orm::DrogonDbException& e) {
                            std::cerr << "save ai reply db error: " << e.base().what() << std::endl;
                        }

                        Json::Value data;
                        data["conversation_id"] = static_cast<Json::Int64>(conversationId);
                        data["reply"] = reply;

                        callback(jsonResp(0, "success", data));
                    }
                );
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "ai chat db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/ai/conversations",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(13000, errorMessage));
                return;
            }

            try {
                auto rows = appctx::db->execSqlSync(
                    "SELECT id, title, created_at, updated_at "
                    "FROM ai_conversations "
                    "WHERE user_id = ? "
                    "ORDER BY updated_at DESC",
                    userId
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    Json::Value item;
                    item["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());

                    if (!row["title"].isNull()) {
                        item["title"] = row["title"].as<std::string>();
                    } else {
                        item["title"] = "";
                    }

                    if (!row["created_at"].isNull()) {
                        item["created_at"] = row["created_at"].as<std::string>();
                    }

                    if (!row["updated_at"].isNull()) {
                        item["updated_at"] = row["updated_at"].as<std::string>();
                    }

                    list.append(item);
                }

                Json::Value data;
                data["list"] = list;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "list ai conversations db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/ai/messages",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(13000, errorMessage));
                return;
            }

            int64_t conversationId = 0;

            if (!parsePositiveInt64(req->getParameter("conversation_id"), conversationId)) {
                callback(jsonResp(13006, "Invalid conversation_id"));
                return;
            }

            try {
                auto ownerRows = appctx::db->execSqlSync(
                    "SELECT id FROM ai_conversations WHERE id = ? AND user_id = ?",
                    conversationId,
                    userId
                );

                if (ownerRows.empty()) {
                    callback(jsonResp(13007, "Conversation not found"));
                    return;
                }

                auto rows = appctx::db->execSqlSync(
                    "SELECT id, role, content, created_at "
                    "FROM ai_messages "
                    "WHERE conversation_id = ? "
                    "ORDER BY id ASC",
                    conversationId
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    Json::Value item;
                    item["id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                    item["role"] = row["role"].as<std::string>();
                    item["content"] = row["content"].as<std::string>();

                    if (!row["created_at"].isNull()) {
                        item["created_at"] = row["created_at"].as<std::string>();
                    }

                    list.append(item);
                }

                Json::Value data;
                data["conversation_id"] = static_cast<Json::Int64>(conversationId);
                data["list"] = list;

                callback(jsonResp(0, "success", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "list ai messages db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Get}
    );

    // 删除AI会话
    app().registerHandler(
        "/api/ai/conversations/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& conversationIdText) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(13000, errorMessage));
                return;
            }

            int64_t conversationId = 0;

            if (!parsePositiveInt64(conversationIdText, conversationId)) {
                callback(jsonResp(13006, "Invalid conversation_id"));
                return;
            }

            try {
                // 验证会话所有权
                auto ownerRows = appctx::db->execSqlSync(
                    "SELECT id FROM ai_conversations WHERE id = ? AND user_id = ?",
                    conversationId,
                    userId
                );

                if (ownerRows.empty()) {
                    callback(jsonResp(13007, "Conversation not found"));
                    return;
                }

                // 删除会话（级联删除消息）
                appctx::db->execSqlSync(
                    "DELETE FROM ai_conversations WHERE id = ? AND user_id = ?",
                    conversationId,
                    userId
                );

                Json::Value data;
                data["conversation_id"] = static_cast<Json::Int64>(conversationId);

                callback(jsonResp(0, "Conversation deleted", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "delete conversation db error: " << e.base().what() << std::endl;
                callback(jsonResp(50001, "Database error"));
            }
        },
        {Delete}
    );
}

} // namespace controllers