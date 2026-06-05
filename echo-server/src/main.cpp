#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <json/json.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <functional>
#include <iostream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <drogon/nosql/RedisClient.h>

static drogon::orm::DbClientPtr g_db;
static drogon::HttpClientPtr g_aiClient;
static std::atomic<uint64_t> g_uploadSequence{0};
static drogon::nosql::RedisClientPtr g_redis;
std::string getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool isSafeMediaFileName(const std::string& fileName) {
    if (fileName.empty()) {
        return false;
    }

    if (fileName.find("..") != std::string::npos ||
        fileName.find('/') != std::string::npos ||
        fileName.find('\\') != std::string::npos) {
        return false;
    }

    return std::all_of(
        fileName.begin(),
        fileName.end(),
        [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        }
    );
}

bool parsePositiveInt64(const std::string& value, int64_t& out) {
    try {
        size_t pos = 0;
        long long parsed = std::stoll(value, &pos);
        if (pos != value.size() || parsed <= 0) {
            return false;
        }
        out = static_cast<int64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseJsonInt64(const Json::Value& json, const std::string& key, int64_t& out) {
    if (!json.isMember(key)) {
        return false;
    }

    const auto& value = json[key];

    try {
        if (value.isString()) {
            return parsePositiveInt64(value.asString(), out);
        }

        if (value.isInt64() || value.isInt() || value.isUInt() || value.isUInt64()) {
            out = value.asInt64();
            return out > 0;
        }
    } catch (...) {
        return false;
    }

    return false;
}

bool parseOptionalJsonInt64(const Json::Value& json, const std::string& key, int64_t& out) {
    out = 0;

    if (!json.isMember(key) || json[key].isNull()) {
        return true;
    }

    const auto& value = json[key];

    try {
        if (value.isString()) {
            std::string text = value.asString();
            if (text.empty()) {
                out = 0;
                return true;
            }

            size_t pos = 0;
            long long parsed = std::stoll(text, &pos);
            if (pos != text.size() || parsed < 0) {
                return false;
            }

            out = static_cast<int64_t>(parsed);
            return true;
        }

        if (value.isInt64() || value.isInt() || value.isUInt() || value.isUInt64()) {
            out = value.asInt64();
            return out >= 0;
        }
    } catch (...) {
        return false;
    }

    return false;
}

bool getUserIdFromRequest(
    const drogon::HttpRequestPtr& req,
    int64_t& userId,
    std::string& errorMessage
) {
    std::string auth = req->getHeader("Authorization");

    if (auth.empty()) {
        auth = req->getHeader("authorization");
    }

    const std::string bearerPrefix = "Bearer ";
    const std::string tokenPrefix = "demo_token_";

    if (auth.empty()) {
        errorMessage = "Missing Authorization header";
        return false;
    }

    if (auth.rfind(bearerPrefix, 0) != 0) {
        errorMessage = "Invalid Authorization format. Use Bearer token";
        return false;
    }

    std::string token = auth.substr(bearerPrefix.length());

    if (token.rfind(tokenPrefix, 0) != 0) {
        errorMessage = "Invalid token format";
        return false;
    }

    std::string userIdStr = token.substr(tokenPrefix.length());

    if (!parsePositiveInt64(userIdStr, userId)) {
        errorMessage = "Invalid user id in token";
        return false;
    }

    return true;
}

bool getOptionalUserIdFromRequest(const drogon::HttpRequestPtr& req, int64_t& userId) {
    userId = 0;

    std::string authorization = req->getHeader("Authorization");
    if (authorization.empty()) {
        authorization = req->getHeader("authorization");
    }

    if (authorization.empty()) {
        return true;
    }

    std::string errorMessage;
    if (!getUserIdFromRequest(req, userId, errorMessage)) {
        userId = 0;
    }

    return true;
}

drogon::HttpResponsePtr makeJsonResponse(
    int code,
    const std::string& message,
    const Json::Value& data = Json::Value()
) {
    Json::Value result;
    result["code"] = code;
    result["message"] = message;

    if (!data.isNull()) {
        result["data"] = data;
    }

    Json::StreamWriterBuilder builder;
    builder["emitUTF8"] = true;
    builder["indentation"] = "";

    std::string body = Json::writeString(builder, result);

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(body);

    return resp;
}

std::string jsonToString(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["emitUTF8"] = true;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

bool readPagination(
    const drogon::HttpRequestPtr& req,
    int& page,
    int& pageSize,
    std::string& errorMessage,
    int defaultPageSize = 20
) {
    page = 1;
    pageSize = defaultPageSize;

    std::string pageStr = req->getParameter("page");
    std::string pageSizeStr = req->getParameter("page_size");

    try {
        if (!pageStr.empty()) {
            page = std::stoi(pageStr);
        }
        if (!pageSizeStr.empty()) {
            pageSize = std::stoi(pageSizeStr);
        }
    } catch (...) {
        errorMessage = "Invalid pagination parameters";
        return false;
    }

    if (page <= 0) {
        errorMessage = "page must be greater than 0";
        return false;
    }

    if (pageSize <= 0) {
        errorMessage = "page_size must be greater than 0";
        return false;
    }

    if (pageSize > 50) {
        pageSize = 50;
    }

    return true;
}

Json::Value makePaginationData(
    int64_t currentUserId,
    int page,
    int pageSize,
    int64_t totalCount,
    const std::string& listKey,
    const Json::Value& list
) {
    Json::Value data;
    int64_t totalPages = totalCount > 0 ? (totalCount + pageSize - 1) / pageSize : 0;

    data["is_login"] = currentUserId > 0;
    data["current_user_id"] = static_cast<Json::Int64>(currentUserId);
    data["page"] = page;
    data["page_size"] = pageSize;
    data["total_count"] = static_cast<Json::Int64>(totalCount);
    data["total_pages"] = static_cast<Json::Int64>(totalPages);
    data["has_more"] = static_cast<int64_t>(page) * pageSize < totalCount;
    data[listKey] = list;

    return data;
}

Json::Value rowToPostJson(const drogon::orm::Row& row, int64_t currentUserId) {
    int64_t postUserId = row["user_id"].as<int64_t>();

    Json::Value item;
    item["post_id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
    item["user_id"] = static_cast<Json::Int64>(postUserId);
    item["username"] = row["username"].as<std::string>();
    item["nickname"] = row["nickname"].as<std::string>();
    item["avatar_url"] = row["avatar_url"].as<std::string>();
    item["content"] = row["content"].as<std::string>();
    item["image_url"] = row["image_url"].as<std::string>();
    item["created_at"] = row["created_at"].as<std::string>();
    item["like_count"] = static_cast<Json::Int64>(row["like_count"].as<int64_t>());
    item["comment_count"] = static_cast<Json::Int64>(row["comment_count"].as<int64_t>());
    item["is_owner"] = currentUserId > 0 && currentUserId == postUserId;
    item["liked"] = row["liked"].as<int>() == 1;
    item["can_edit"] = currentUserId > 0 && currentUserId == postUserId;
    item["can_delete"] = currentUserId > 0 && currentUserId == postUserId;

    return item;
}

std::string postListSql(const std::string& whereClause) {
    return
        "SELECT p.id, p.user_id, u.username, "
        "COALESCE(u.nickname, '') AS nickname, "
        "COALESCE(u.avatar_url, '') AS avatar_url, "
        "p.content, COALESCE(p.image_url, '') AS image_url, "
        "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
        "COALESCE(lc.like_count, 0) AS like_count, "
        "COALESCE(cc.comment_count, 0) AS comment_count, "
        "CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked "
        "FROM posts p "
        "JOIN users u ON u.id = p.user_id "
        "LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ? "
        "LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id "
        "LEFT JOIN (SELECT post_id, COUNT(*) AS comment_count FROM comments GROUP BY post_id) cc ON cc.post_id = p.id " +
        whereClause +
        " ORDER BY p.id DESC LIMIT ? OFFSET ?";
}

Json::Value rowToUserCardJson(const drogon::orm::Row& row, int64_t currentUserId) {
    int64_t itemUserId = row["id"].as<int64_t>();

    Json::Value item;
    item["user_id"] = static_cast<Json::Int64>(itemUserId);
    item["username"] = row["username"].as<std::string>();
    item["nickname"] = row["nickname"].as<std::string>();
    item["avatar_url"] = row["avatar_url"].as<std::string>();
    item["bio"] = row["bio"].as<std::string>();
    item["followed_at"] = row["followed_at"].as<std::string>();
    item["is_self"] = currentUserId > 0 && currentUserId == itemUserId;
    item["followed_by_me"] = row["followed_by_me"].as<int>() == 1;

    return item;
}

Json::Value makeUserStatsFromRow(const drogon::orm::Row& row) {
    Json::Value stats;
    stats["post_count"] = static_cast<Json::Int64>(row["post_count"].as<int64_t>());
    stats["media_count"] = static_cast<Json::Int64>(row["media_count"].as<int64_t>());
    stats["follower_count"] = static_cast<Json::Int64>(row["follower_count"].as<int64_t>());
    stats["following_count"] = static_cast<Json::Int64>(row["following_count"].as<int64_t>());
    return stats;
}

void queryUserStatsAsync(
    int64_t userId,
    std::function<void(Json::Value)>&& onSuccess,
    std::function<void(const drogon::orm::DrogonDbException&)>&& onError
) {
    g_db->execSqlAsync(
        "SELECT "
        "(SELECT COUNT(*) FROM posts WHERE user_id = ?) AS post_count, "
        "(SELECT COUNT(*) FROM posts WHERE user_id = ? AND image_url IS NOT NULL AND image_url <> '') AS media_count, "
        "(SELECT COUNT(*) FROM user_follows WHERE following_id = ?) AS follower_count, "
        "(SELECT COUNT(*) FROM user_follows WHERE follower_id = ?) AS following_count",
        [onSuccess = std::move(onSuccess)](const drogon::orm::Result& rows) mutable {
            if (rows.empty()) {
                Json::Value stats;
                stats["post_count"] = 0;
                stats["media_count"] = 0;
                stats["follower_count"] = 0;
                stats["following_count"] = 0;
                onSuccess(stats);
                return;
            }
            onSuccess(makeUserStatsFromRow(rows[0]));
        },
        [onError = std::move(onError)](const drogon::orm::DrogonDbException& e) mutable {
            onError(e);
        },
        userId,
        userId,
        userId,
        userId
    );
}

int main() {
    using namespace drogon;

    g_db = drogon::orm::DbClient::newMysqlClient(
        "host=127.0.0.1 port=3306 dbname=echo_app user=echo_user password=123456",
        4
    );

    g_aiClient = drogon::HttpClient::newHttpClient("http://127.0.0.1:18080");

    g_redis = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379)
    );

    std::filesystem::create_directories("./uploads");

    app().registerHandler(
        "/ping",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            (void)req;
            Json::Value data;
            data["service"] = "echo-server";
            callback(makeJsonResponse(0, "pong", data));
        },
        {Get}
    );
    app().registerHandler(
        "/api/debug/redis",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            (void)req;

            g_redis->execCommandAsync(
                [callback](const drogon::nosql::RedisResult& result) {
                    Json::Value data;
                    data["redis"] = result.asString();
                    callback(makeJsonResponse(0, "Redis OK", data));
                },
                [callback](const std::exception& e) {
                    Json::Value data;
                    data["error"] = e.what();
                    callback(makeJsonResponse(12001, "Redis error", data));
                },
                "PING"
            );
        },
        {Get}
    );
    app().registerHandler(
        "/api/upload/media",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(makeJsonResponse(10000, errorMessage));
                return;
            }

            try {
                MultiPartParser parser;

                if (parser.parse(req) != 0) {
                    callback(makeJsonResponse(10001, "Failed to parse upload request"));
                    return;
                }

                const auto& files = parser.getFiles();

                if (files.empty()) {
                    callback(makeJsonResponse(10002, "No file uploaded"));
                    return;
                }

                if (files.size() != 1) {
                    callback(makeJsonResponse(10003, "Only one file can be uploaded at a time"));
                    return;
                }

                const auto& file = files[0];

                if (file.fileLength() == 0) {
                    callback(makeJsonResponse(10004, "Uploaded file is empty"));
                    return;
                }

                const size_t maxMediaSize = 50 * 1024 * 1024;

                if (file.fileLength() > maxMediaSize) {
                    callback(makeJsonResponse(10005, "File size must not exceed 50MB"));
                    return;
                }

                std::string extension(file.getFileExtension());
                std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });

                bool isImage = extension == "jpg" || extension == "jpeg" || extension == "png";
                bool isVideo = extension == "mp4";

                if (!isImage && !isVideo) {
                    callback(makeJsonResponse(10006, "Only jpg, jpeg, png, and mp4 files are supported"));
                    return;
                }

                auto now = std::chrono::system_clock::now();
                auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                uint64_t sequence = ++g_uploadSequence;

                std::string storedFileName =
                    "media_" + std::to_string(userId) + "_" +
                    std::to_string(milliseconds) + "_" +
                    std::to_string(sequence) + "." + extension;

                std::filesystem::path uploadDir = std::filesystem::absolute("./uploads");
                std::filesystem::create_directories(uploadDir);
                std::filesystem::path savePath = uploadDir / storedFileName;
                file.saveAs(savePath.string());

                std::string mediaUrl = "/api/media/file?name=" + storedFileName;

                Json::Value data;
                data["original_name"] = file.getFileName();
                data["file_name"] = storedFileName;
                data["media_url"] = mediaUrl;
                data["media_type"] = isVideo ? "video" : "image";
                data["extension"] = extension;
                data["size"] = static_cast<Json::UInt64>(file.fileLength());

                callback(makeJsonResponse(0, "Upload succeeded", data));
            } catch (const std::exception& e) {
                std::cerr << "upload error: " << e.what() << std::endl;
                callback(makeJsonResponse(10007, "Failed to save file"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/media/file",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string fileName = req->getParameter("name");

            if (!isSafeMediaFileName(fileName)) {
                callback(makeJsonResponse(10101, "Invalid file name"));
                return;
            }

            std::filesystem::path uploadDir = std::filesystem::absolute("./uploads");
            std::filesystem::path filePath = uploadDir / fileName;

            if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
                callback(makeJsonResponse(10102, "File not found"));
                return;
            }

            std::string extension = filePath.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (extension == ".mp4") {
                callback(HttpResponse::newFileResponse(filePath.string(), "", CT_CUSTOM, "video/mp4", req));
                return;
            }

            callback(HttpResponse::newFileResponse(filePath.string(), "", CT_NONE, "", req));
        },
        {Get}
    );

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
                auto exists = g_db->execSqlSync("SELECT id FROM users WHERE username = ?", username);
                if (!exists.empty()) {
                    callback(makeJsonResponse(1005, "username already exists"));
                    return;
                }

                auto result = g_db->execSqlSync(
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
                auto rows = g_db->execSqlSync(
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

    app().registerHandler(
        "/api/ai/chat",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(makeJsonResponse(12000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(12001, "Request body must be JSON"));
                return;
            }

            std::string message = (*json).get("message", "").asString();

            if (message.empty()) {
                callback(makeJsonResponse(12002, "message is required"));
                return;
            }

            if (message.length() > 2000) {
                callback(makeJsonResponse(12003, "message must not exceed 2000 characters"));
                return;
            }

            int64_t conversationId = 0;
            if (!parseOptionalJsonInt64(*json, "conversation_id", conversationId)) {
                callback(makeJsonResponse(12004, "Invalid conversation_id"));
                return;
            }

            try {
                if (conversationId > 0) {
                    auto conversationRows = g_db->execSqlSync(
                        "SELECT id FROM ai_conversations WHERE id = ? AND user_id = ?",
                        conversationId,
                        userId
                    );

                    if (conversationRows.empty()) {
                        callback(makeJsonResponse(12005, "Conversation not found"));
                        return;
                    }
                } else {
                        std::string title = message.substr(0, 30); // 截取前30字符作为标题
                        auto result = g_db->execSqlSync(
                            "INSERT INTO ai_conversations(user_id, title) VALUES(?, ?)",
                            userId,
                            title
                        );

                    conversationId = static_cast<int64_t>(result.insertId());
                    if (conversationId <= 0) {
                        callback(makeJsonResponse(12006, "Failed to create conversation"));
                        return;
                    }
                }

                g_db->execSqlSync(
                    "INSERT INTO ai_messages(conversation_id, user_id, role, content) VALUES(?, ?, ?, ?)",
                    conversationId,
                    userId,
                    "user",
                    message
                );

                auto historyRows = g_db->execSqlSync(
                    "SELECT role, content FROM ("
                    "SELECT id, role, content FROM ai_messages "
                    "WHERE conversation_id = ? AND user_id = ? "
                    "ORDER BY id DESC LIMIT 10"
                    ") t ORDER BY id ASC",
                    conversationId,
                    userId
                );

                Json::Value aiBody;
                aiBody["model"] = "local-model";
                aiBody["temperature"] = 0.7;
                aiBody["max_tokens"] = 512;

                Json::Value messages(Json::arrayValue);

                Json::Value systemMessage;
                systemMessage["role"] = "system";
                systemMessage["content"] =
                    "你是一个社区AI助手，回答要简洁、友好、积极，适合社区用户。"
                    "如果用户让你写动态、评论或回复，请给出自然、可直接使用的中文内容。";
                messages.append(systemMessage);

                for (const auto& row : historyRows) {
                    std::string role = row["role"].as<std::string>();
                    std::string content = row["content"].as<std::string>();

                    if (role != "user" && role != "assistant") {
                        continue;
                    }

                    Json::Value item;
                    item["role"] = role;
                    item["content"] = content;
                    messages.append(item);
                }

                aiBody["messages"] = messages;

                auto aiReq = HttpRequest::newHttpJsonRequest(aiBody);
                aiReq->setMethod(Post);
                aiReq->setPath("/v1/chat/completions");

                g_aiClient->sendRequest(
                    aiReq,
                    [callback, conversationId, userId](ReqResult result, const HttpResponsePtr& aiResp) {
                        if (result != ReqResult::Ok || !aiResp) {
                            callback(makeJsonResponse(12010, "AI service unavailable"));
                            return;
                        }

                        if (aiResp->statusCode() != k200OK) {
                            std::cerr << "AI service status error: "
                                    << aiResp->statusCode()
                                    << ", body: "
                                    << aiResp->body()
                                    << std::endl;

                            callback(makeJsonResponse(12011, "AI service returned an error"));
                            return;
                        }

                        auto aiJson = aiResp->getJsonObject();
                        if (!aiJson) {
                            std::cerr << "AI response is not JSON: " << aiResp->body() << std::endl;
                            callback(makeJsonResponse(12012, "Invalid AI response"));
                            return;
                        }

                        std::string reply;

                        try {
                            const auto& choices = (*aiJson)["choices"];

                            if (choices.isArray() && choices.size() > 0) {
                                const auto& firstChoice = choices[0];

                                if (firstChoice.isMember("message")) {
                                    reply = firstChoice["message"].get("content", "").asString();
                                } else if (firstChoice.isMember("text")) {
                                    reply = firstChoice.get("text", "").asString();
                                }
                            }
                        } catch (...) {
                            reply.clear();
                        }

                        if (reply.empty()) {
                            std::cerr << "AI response missing reply: " << aiResp->body() << std::endl;
                            callback(makeJsonResponse(12013, "AI response missing reply"));
                            return;
                        }

                        try {
                            g_db->execSqlSync(
                                "INSERT INTO ai_messages(conversation_id, user_id, role, content) VALUES(?, ?, ?, ?)",
                                conversationId,
                                userId,
                                "assistant",
                                reply
                            );

                            g_db->execSqlSync(
                                "UPDATE ai_conversations SET updated_at = CURRENT_TIMESTAMP WHERE id = ? AND user_id = ?",
                                conversationId,
                                userId
                            );

                            Json::Value data;
                            data["conversation_id"] = static_cast<Json::Int64>(conversationId);
                            data["reply"] = reply;

                            callback(makeJsonResponse(0, "OK", data));
                        } catch (const drogon::orm::DrogonDbException& e) {
                            std::cerr << "database error after AI reply: " << e.base().what() << std::endl;
                            callback(makeJsonResponse(5001, "Database error"));
                        }
                    },
                    60.0
                );
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
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
                callback(makeJsonResponse(12100, errorMessage));
                return;
            }

            try {
                auto rows = g_db->execSqlSync(
                    "SELECT id, title, DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s') AS updated_at "
                    "FROM ai_conversations "
                    "WHERE user_id = ? "
                    "ORDER BY updated_at DESC, id DESC "
                    "LIMIT 50",
                    userId
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    Json::Value item;
                    item["conversation_id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                    item["title"] = row["title"].as<std::string>();
                    item["updated_at"] = row["updated_at"].as<std::string>();
                    list.append(item);
                }

                Json::Value data;
                data["conversations"] = list;
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
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
                callback(makeJsonResponse(12200, errorMessage));
                return;
            }

            int64_t conversationId = 0;
            if (!parsePositiveInt64(req->getParameter("conversation_id"), conversationId)) {
                callback(makeJsonResponse(12201, "Invalid conversation_id"));
                return;
            }

            try {
                auto conversationRows = g_db->execSqlSync(
                    "SELECT id FROM ai_conversations WHERE id = ? AND user_id = ?",
                    conversationId,
                    userId
                );

                if (conversationRows.empty()) {
                    callback(makeJsonResponse(12202, "Conversation not found"));
                    return;
                }

                auto rows = g_db->execSqlSync(
                    "SELECT id, role, content, DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
                    "FROM ai_messages "
                    "WHERE conversation_id = ? AND user_id = ? "
                    "ORDER BY id ASC "
                    "LIMIT 200",
                    conversationId,
                    userId
                );

                Json::Value list(Json::arrayValue);

                for (const auto& row : rows) {
                    Json::Value item;
                    item["message_id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                    item["role"] = row["role"].as<std::string>();
                    item["content"] = row["content"].as<std::string>();
                    item["created_at"] = row["created_at"].as<std::string>();
                    list.append(item);
                }

                Json::Value data;
                data["conversation_id"] = static_cast<Json::Int64>(conversationId);
                data["messages"] = list;

                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );





    app().registerHandler(
        "/api/posts/create",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(makeJsonResponse(3000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(3001, "Request body must be JSON"));
                return;
            }

            std::string content = (*json).get("content", "").asString();
            std::string imageUrl = (*json).get("image_url", "").asString();

            if (content.empty()) {
                callback(makeJsonResponse(3003, "content is required"));
                return;
            }

            if (content.length() > 2000) {
                callback(makeJsonResponse(3004, "content must not exceed 2000 characters"));
                return;
            }

            if (imageUrl.length() > 500) {
                callback(makeJsonResponse(3007, "image_url must not exceed 500 characters"));
                return;
            }

            try {
                auto userRows = g_db->execSqlSync("SELECT id FROM users WHERE id = ?", userId);
                if (userRows.empty()) {
                    callback(makeJsonResponse(3005, "User not found"));
                    return;
                }

                auto result = g_db->execSqlSync(
                    "INSERT INTO posts(user_id, content, image_url) VALUES(?, ?, NULLIF(?, ''))",
                    userId,
                    content,
                    imageUrl
                );

                uint64_t postId = result.insertId();
                if (postId == 0) {
                    callback(makeJsonResponse(3006, "Failed to create post"));
                    return;
                }

                Json::Value data;
                data["post_id"] = static_cast<Json::Int64>(postId);
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["content"] = content;
                data["image_url"] = imageUrl;
                data["created_at"] = getCurrentTimeString();

                callback(makeJsonResponse(0, "Post created", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );
    app().registerHandler(
        "/api/posts/latest",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(4301, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            std::string cacheKey =
                "cache:posts:latest:page:" + std::to_string(page) +
                ":size:" + std::to_string(pageSize) +
                ":user:" + std::to_string(currentUserId);

            g_redis->execCommandAsync(
                [callback, currentUserId, page, pageSize, offset, cacheKey](const drogon::nosql::RedisResult& cacheResult) {
                    if (!cacheResult.isNil()) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k200OK);
                        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        resp->setBody(cacheResult.asString());
                        callback(resp);
                        return;
                    }

                    g_db->execSqlAsync(
                        "SELECT COUNT(*) AS total_count FROM posts",
                        [callback, currentUserId, page, pageSize, offset, cacheKey](const drogon::orm::Result& countRows) {
                            int64_t totalCount = 0;
                            if (!countRows.empty()) {
                                totalCount = countRows[0]["total_count"].as<int64_t>();
                            }

                            g_db->execSqlAsync(
                                postListSql(""),
                                [callback, currentUserId, page, pageSize, totalCount, cacheKey](const drogon::orm::Result& rows) {
                                    Json::Value list(Json::arrayValue);
                                    for (const auto& row : rows) {
                                        list.append(rowToPostJson(row, currentUserId));
                                    }

                                    Json::Value data = makePaginationData(
                                        currentUserId,
                                        page,
                                        pageSize,
                                        totalCount,
                                        "posts",
                                        list
                                    );

                                    auto response = makeJsonResponse(0, "OK", data);
                                    std::string responseBody(response->body());
                                    g_redis->execCommandAsync(
                                        [](const drogon::nosql::RedisResult& setResult) {
                                            (void)setResult;
                                        },
                                        [](const std::exception& e) {
                                            std::cerr << "redis set cache error: " << e.what() << std::endl;
                                        },
                                        "SETEX %s 30 %s",
                                        cacheKey.c_str(),
                                        responseBody.c_str()
                                    );

                                    callback(response);
                                },
                                [callback](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "database error posts latest list: " << e.base().what() << std::endl;
                                    callback(makeJsonResponse(5001, "Database error"));
                                },
                                currentUserId,
                                pageSize,
                                offset
                            );
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "database error posts latest count: " << e.base().what() << std::endl;
                            callback(makeJsonResponse(5001, "Database error"));
                        }
                    );
                },
                [callback, currentUserId, page, pageSize, offset](const std::exception& e) {
                    std::cerr << "redis get cache error: " << e.what() << std::endl;

                    g_db->execSqlAsync(
                        "SELECT COUNT(*) AS total_count FROM posts",
                        [callback, currentUserId, page, pageSize, offset](const drogon::orm::Result& countRows) {
                            int64_t totalCount = 0;
                            if (!countRows.empty()) {
                                totalCount = countRows[0]["total_count"].as<int64_t>();
                            }

                            g_db->execSqlAsync(
                                postListSql(""),
                                [callback, currentUserId, page, pageSize, totalCount](const drogon::orm::Result& rows) {
                                    Json::Value list(Json::arrayValue);
                                    for (const auto& row : rows) {
                                        list.append(rowToPostJson(row, currentUserId));
                                    }

                                    Json::Value data = makePaginationData(
                                        currentUserId,
                                        page,
                                        pageSize,
                                        totalCount,
                                        "posts",
                                        list
                                    );

                                    callback(makeJsonResponse(0, "OK", data));
                                },
                                [callback](const drogon::orm::DrogonDbException& e) {
                                    std::cerr << "database error posts latest list fallback: " << e.base().what() << std::endl;
                                    callback(makeJsonResponse(5001, "Database error"));
                                },
                                currentUserId,
                                pageSize,
                                offset
                            );
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            std::cerr << "database error posts latest count fallback: " << e.base().what() << std::endl;
                            callback(makeJsonResponse(5001, "Database error"));
                        }
                    );
                },
                "GET %s",
                cacheKey.c_str()
            );
        },
        {Get}
    );

    // 获取热门帖子(按点赞数排序)
    app().registerHandler(
        "/api/posts/hot",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(4301, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                // 获取总数
                auto countRows = g_db->execSqlSync("SELECT COUNT(*) AS total_count FROM posts");
                int64_t totalCount = 0;
                if (!countRows.empty()) {
                    totalCount = countRows[0]["total_count"].as<int64_t>();
                }

                // 按点赞数排序查询帖子
                auto rows = g_db->execSqlSync(
                    "SELECT p.id, p.user_id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "p.content, COALESCE(p.image_url, '') AS image_url, "
                    "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
                    "COALESCE(lc.like_count, 0) AS like_count, "
                    "COALESCE(cc.comment_count, 0) AS comment_count, "
                    "CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked "
                    "FROM posts p "
                    "JOIN users u ON u.id = p.user_id "
                    "LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ? "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS comment_count FROM comments GROUP BY post_id) cc ON cc.post_id = p.id "
                    "ORDER BY like_count DESC, p.created_at DESC LIMIT ? OFFSET ?",
                    currentUserId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToPostJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(
                    currentUserId,
                    page,
                    pageSize,
                    totalCount,
                    "posts",
                    list
                );

                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error posts hot list: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    // 获取关注用户的帖子
    app().registerHandler(
        "/api/posts/following",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            // 必须登录才能查看关注流
            if (currentUserId == 0) {
                callback(makeJsonResponse(8000, "Please login to view following feed"));
                return;
            }

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(4301, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                // 获取总数
                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM posts p "
                    "INNER JOIN user_follows f ON f.following_id = p.user_id "
                    "WHERE f.follower_id = ?",
                    currentUserId
                );
                int64_t totalCount = 0;
                if (!countRows.empty()) {
                    totalCount = countRows[0]["total_count"].as<int64_t>();
                }

                // 查询关注用户的帖子
                auto rows = g_db->execSqlSync(
                    "SELECT p.id, p.user_id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "p.content, COALESCE(p.image_url, '') AS image_url, "
                    "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
                    "COALESCE(lc.like_count, 0) AS like_count, "
                    "COALESCE(cc.comment_count, 0) AS comment_count, "
                    "CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked "
                    "FROM posts p "
                    "JOIN users u ON u.id = p.user_id "
                    "INNER JOIN user_follows f ON f.following_id = p.user_id "
                    "LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ? "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS comment_count FROM comments GROUP BY post_id) cc ON cc.post_id = p.id "
                    "WHERE f.follower_id = ? "
                    "ORDER BY p.created_at DESC LIMIT ? OFFSET ?",
                    currentUserId,
                    currentUserId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToPostJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(
                    currentUserId,
                    page,
                    pageSize,
                    totalCount,
                    "posts",
                    list
                );

                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error posts following list: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/posts/user",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int64_t userId = 0;
            if (!parsePositiveInt64(req->getParameter("user_id"), userId)) {
                callback(makeJsonResponse(4001, "Invalid user_id"));
                return;
            }

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(4401, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto userRows = g_db->execSqlSync("SELECT id FROM users WHERE id = ?", userId);
                if (userRows.empty()) {
                    callback(makeJsonResponse(4004, "User not found"));
                    return;
                }

                auto countRows = g_db->execSqlSync("SELECT COUNT(*) AS total_count FROM posts WHERE user_id = ?", userId);
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto rows = g_db->execSqlSync(
                    postListSql("WHERE p.user_id = ?"),
                    currentUserId,
                    userId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToPostJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "posts", list);
                data["user_id"] = static_cast<Json::Int64>(userId);
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/posts/detail",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int64_t postId = 0;
            if (!parsePositiveInt64(req->getParameter("post_id"), postId)) {
                callback(makeJsonResponse(4501, "Invalid post_id"));
                return;
            }

            // 第一步：异步查询帖子主体详情
            g_db->execSqlAsync(
                "SELECT p.id, p.user_id, u.username, "
                "COALESCE(u.nickname, '') AS nickname, "
                "COALESCE(u.avatar_url, '') AS avatar_url, "
                "p.content, COALESCE(p.image_url, '') AS image_url, "
                "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
                "COALESCE(lc.like_count, 0) AS like_count, "
                "COALESCE(cc.comment_count, 0) AS comment_count, "
                "CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked "
                "FROM posts p "
                "JOIN users u ON u.id = p.user_id "
                "LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ? "
                "LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id "
                "LEFT JOIN (SELECT post_id, COUNT(*) AS comment_count FROM comments GROUP BY post_id) cc ON cc.post_id = p.id "
                "WHERE p.id = ?",
                [callback, currentUserId, postId](const drogon::orm::Result& postRows) {
                    // 帖子查询成功回调
                    if (postRows.empty()) {
                        callback(makeJsonResponse(4504, "Post not found"));
                        return;
                    }

                    // 暂存帖子行数据，准备传给下一层 Lambda
                    auto postRow = postRows[0];

                    // 第二步：嵌套异步查询评论列表（LIMIT 100）
                    g_db->execSqlAsync(
                        "SELECT c.id, c.post_id, c.user_id, u.username, "
                        "COALESCE(u.nickname, '') AS nickname, "
                        "COALESCE(u.avatar_url, '') AS avatar_url, "
                        "c.content, DATE_FORMAT(c.created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
                        "FROM comments c "
                        "JOIN users u ON u.id = c.user_id "
                        "WHERE c.post_id = ? ORDER BY c.id ASC LIMIT 100",
                        [callback, currentUserId, postRow](const drogon::orm::Result& commentRows) {
                            // 评论查询成功回调
                            Json::Value comments(Json::arrayValue);
                            for (const auto& row : commentRows) {
                                int64_t commentUserId = row["user_id"].as<int64_t>();
                                Json::Value item;
                                item["comment_id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                                item["post_id"] = static_cast<Json::Int64>(row["post_id"].as<int64_t>());
                                item["user_id"] = static_cast<Json::Int64>(commentUserId);
                                item["username"] = row["username"].as<std::string>();
                                item["nickname"] = row["nickname"].as<std::string>();
                                item["avatar_url"] = row["avatar_url"].as<std::string>();
                                item["content"] = row["content"].as<std::string>();
                                item["created_at"] = row["created_at"].as<std::string>();
                                item["is_owner"] = currentUserId > 0 && currentUserId == commentUserId;
                                comments.append(item);
                            }

                            // 组装最终数据
                            Json::Value data;
                            data["post"] = rowToPostJson(postRow, currentUserId);
                            data["comments"] = comments;
                            
                            // 响应前端
                            callback(makeJsonResponse(0, "OK", data));
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            // 评论查询失败回调
                            std::cerr << "database error (comments): " << e.base().what() << std::endl;
                            callback(makeJsonResponse(5001, "Database error"));
                        },
                        postId
                    );
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    // 帖子查询失败回调
                    std::cerr << "database error (post detail): " << e.base().what() << std::endl;
                    callback(makeJsonResponse(5001, "Database error"));
                },
                currentUserId,
                postId
            );
        },
        {Get}
    );
    
    app().registerHandler(
        "/api/posts/like/toggle",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;
            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(6000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(6001, "Request body must be JSON"));
                return;
            }

            int64_t postId = 0;
            if (!parseJsonInt64(*json, "post_id", postId)) {
                callback(makeJsonResponse(6002, "Invalid post_id"));
                return;
            }

            try {
                auto postRows = g_db->execSqlSync("SELECT id FROM posts WHERE id = ?", postId);
                if (postRows.empty()) {
                    callback(makeJsonResponse(6003, "Post not found"));
                    return;
                }

                auto likeRows = g_db->execSqlSync(
                    "SELECT post_id FROM post_likes WHERE post_id = ? AND user_id = ?",
                    postId,
                    currentUserId
                );

                bool liked = false;
                if (likeRows.empty()) {
                    g_db->execSqlSync(
                        "INSERT INTO post_likes(post_id, user_id) VALUES(?, ?)",
                        postId,
                        currentUserId
                    );
                    liked = true;
                } else {
                    g_db->execSqlSync(
                        "DELETE FROM post_likes WHERE post_id = ? AND user_id = ?",
                        postId,
                        currentUserId
                    );
                    liked = false;
                }

                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS like_count FROM post_likes WHERE post_id = ?",
                    postId
                );

                Json::Value data;
                data["post_id"] = static_cast<Json::Int64>(postId);
                data["liked"] = liked;
                data["like_count"] = static_cast<Json::Int64>(countRows[0]["like_count"].as<int64_t>());

                callback(makeJsonResponse(0, liked ? "Liked" : "Unliked", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/posts/comment",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;
            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(7000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(7001, "Request body must be JSON"));
                return;
            }

            int64_t postId = 0;
            if (!parseJsonInt64(*json, "post_id", postId)) {
                callback(makeJsonResponse(7002, "Invalid post_id"));
                return;
            }

            std::string content = (*json).get("content", "").asString();
            if (content.empty()) {
                callback(makeJsonResponse(7003, "content is required"));
                return;
            }

            if (content.length() > 1000) {
                callback(makeJsonResponse(7004, "content must not exceed 1000 characters"));
                return;
            }

            try {
                auto postRows = g_db->execSqlSync("SELECT id FROM posts WHERE id = ?", postId);
                if (postRows.empty()) {
                    callback(makeJsonResponse(7005, "Post not found"));
                    return;
                }

                auto result = g_db->execSqlSync(
                    "INSERT INTO comments(post_id, user_id, content) VALUES(?, ?, ?)",
                    postId,
                    currentUserId,
                    content
                );

                uint64_t commentId = result.insertId();

                auto rows = g_db->execSqlSync(
                    "SELECT c.id, c.post_id, c.user_id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "c.content, DATE_FORMAT(c.created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
                    "FROM comments c JOIN users u ON u.id = c.user_id WHERE c.id = ?",
                    static_cast<int64_t>(commentId)
                );

                Json::Value data;
                if (!rows.empty()) {
                    data["comment_id"] = static_cast<Json::Int64>(rows[0]["id"].as<int64_t>());
                    data["post_id"] = static_cast<Json::Int64>(rows[0]["post_id"].as<int64_t>());
                    data["user_id"] = static_cast<Json::Int64>(rows[0]["user_id"].as<int64_t>());
                    data["username"] = rows[0]["username"].as<std::string>();
                    data["nickname"] = rows[0]["nickname"].as<std::string>();
                    data["avatar_url"] = rows[0]["avatar_url"].as<std::string>();
                    data["content"] = rows[0]["content"].as<std::string>();
                    data["created_at"] = rows[0]["created_at"].as<std::string>();
                }

                callback(makeJsonResponse(0, "Comment created", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/posts/delete",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;
            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(9000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(9001, "Request body must be JSON"));
                return;
            }

            int64_t postId = 0;
            if (!parseJsonInt64(*json, "post_id", postId)) {
                callback(makeJsonResponse(9002, "Invalid post_id: post_id must be a positive integer"));
                return;
            }

            try {
                auto postRows = g_db->execSqlSync("SELECT id, user_id FROM posts WHERE id = ?", postId);
                if (postRows.empty()) {
                    callback(makeJsonResponse(9003, "Post not found"));
                    return;
                }

                int64_t postUserId = postRows[0]["user_id"].as<int64_t>();
                if (postUserId != currentUserId) {
                    callback(makeJsonResponse(9004, "Only the owner can delete this post"));
                    return;
                }

                g_db->execSqlSync("DELETE FROM comments WHERE post_id = ?", postId);
                g_db->execSqlSync("DELETE FROM post_likes WHERE post_id = ?", postId);
                g_db->execSqlSync("DELETE FROM posts WHERE id = ? AND user_id = ?", postId, currentUserId);

                Json::Value data;
                data["post_id"] = static_cast<Json::Int64>(postId);
                callback(makeJsonResponse(0, "Post deleted", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/users/me",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(makeJsonResponse(8000, errorMessage));
                return;
            }

            try {
                auto rows = g_db->execSqlSync(
                    "SELECT id, username, "
                    "COALESCE(nickname, '') AS nickname, "
                    "COALESCE(avatar_url, '') AS avatar_url, "
                    "COALESCE(bio, '') AS bio, "
                    "COALESCE(cover_image_url, '') AS cover_image_url "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (rows.empty()) {
                    callback(makeJsonResponse(8001, "User not found"));
                    return;
                }

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(rows[0]["id"].as<int64_t>());
                data["username"] = rows[0]["username"].as<std::string>();
                data["nickname"] = rows[0]["nickname"].as<std::string>();
                data["avatar_url"] = rows[0]["avatar_url"].as<std::string>();
                data["bio"] = rows[0]["bio"].as<std::string>();
                data["cover_image_url"] = rows[0]["cover_image_url"].as<std::string>();
                queryUserStatsAsync(
                    userId,
                    [callback, data](Json::Value stats) mutable {
                        data["stats"] = stats;
                        callback(makeJsonResponse(0, "OK", data));
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "database error: " << e.base().what() << std::endl;
                        callback(makeJsonResponse(5001, "Database error"));
                    }
                );
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/users/profile",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);
            

            int64_t userId = 0;
            if (!parsePositiveInt64(req->getParameter("user_id"), userId)) {
                callback(makeJsonResponse(8101, "Invalid user_id"));
                return;
            }

            try {
                auto rows = g_db->execSqlSync(
                    "SELECT id, username, "
                    "COALESCE(nickname, '') AS nickname, "
                    "COALESCE(avatar_url, '') AS avatar_url, "
                    "COALESCE(bio, '') AS bio, "
                    "COALESCE(cover_image_url, '') AS cover_image_url "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (rows.empty()) {
                    callback(makeJsonResponse(8104, "User not found"));
                    return;
                }

                bool isFollowing = false;
                if (currentUserId > 0 && currentUserId != userId) {
                    auto followRows = g_db->execSqlSync(
                        "SELECT id FROM user_follows WHERE follower_id = ? AND following_id = ?",
                        currentUserId,
                        userId
                    );
                    isFollowing = !followRows.empty();
                }

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(rows[0]["id"].as<int64_t>());
                data["username"] = rows[0]["username"].as<std::string>();
                data["nickname"] = rows[0]["nickname"].as<std::string>();
                data["avatar_url"] = rows[0]["avatar_url"].as<std::string>();
                data["bio"] = rows[0]["bio"].as<std::string>();
                data["cover_image_url"] = rows[0]["cover_image_url"].as<std::string>();
                data["is_self"] = currentUserId > 0 && currentUserId == userId;
                data["is_following"] = isFollowing;
                queryUserStatsAsync(
                    userId,
                    [callback, data](Json::Value stats) mutable {
                        data["stats"] = stats;
                        callback(makeJsonResponse(0, "OK", data));
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "database error: " << e.base().what() << std::endl;
                        callback(makeJsonResponse(5001, "Database error"));
                    }
                );
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/users/update-nickname",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(makeJsonResponse(8200, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(8201, "Request body must be JSON"));
                return;
            }

            std::string nickname = (*json).get("nickname", "").asString();
            if (nickname.length() > 60) {
                callback(makeJsonResponse(8203, "nickname must not exceed 60 characters"));
                return;
            }

            try {
                g_db->execSqlSync("UPDATE users SET nickname = NULLIF(?, '') WHERE id = ?", nickname, userId);
                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["nickname"] = nickname;
                callback(makeJsonResponse(0, "Profile updated", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/follows/toggle",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(11000, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(11001, "Request body must be JSON"));
                return;
            }

            int64_t targetUserId = 0;
            if (!parseJsonInt64(*json, "target_user_id", targetUserId)) {
                callback(makeJsonResponse(11002, "Invalid target_user_id"));
                return;
            }

            if (targetUserId == currentUserId) {
                callback(makeJsonResponse(11005, "Cannot follow yourself"));
                return;
            }

            try {
                auto userRows = g_db->execSqlSync("SELECT id FROM users WHERE id = ?", targetUserId);
                if (userRows.empty()) {
                    callback(makeJsonResponse(11006, "Target user not found"));
                    return;
                }

                auto followRows = g_db->execSqlSync(
                    "SELECT id FROM user_follows WHERE follower_id = ? AND following_id = ?",
                    currentUserId,
                    targetUserId
                );

                bool isFollowing = false;
                std::string action;

                if (followRows.empty()) {
                    g_db->execSqlSync(
                        "INSERT INTO user_follows(follower_id, following_id) VALUES(?, ?)",
                        currentUserId,
                        targetUserId
                    );
                    isFollowing = true;
                    action = "follow";
                } else {
                    g_db->execSqlSync(
                        "DELETE FROM user_follows WHERE follower_id = ? AND following_id = ?",
                        currentUserId,
                        targetUserId
                    );
                    isFollowing = false;
                    action = "unfollow";
                }

                Json::Value data;
                data["current_user_id"] = static_cast<Json::Int64>(currentUserId);
                data["target_user_id"] = static_cast<Json::Int64>(targetUserId);
                data["is_following"] = isFollowing;
                data["action"] = action;

                queryUserStatsAsync(
                    targetUserId,
                    [callback, data, isFollowing](Json::Value stats) mutable {
                        data["follower_count"] = stats["follower_count"];
                        data["following_count"] = stats["following_count"];
                        callback(makeJsonResponse(0, isFollowing ? "Followed" : "Unfollowed", data));
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "database error: " << e.base().what() << std::endl;
                        callback(makeJsonResponse(5001, "Database error"));
                    }
                );
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    // 获取帖子的评论列表
    app().registerHandler(
        "/api/comments/post/{post_id}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            // 从路由参数中获取 post_id
            int64_t postId = 0;
            const auto& routingParams = req->getRoutingParameters();
            if (routingParams.empty()) {
                callback(makeJsonResponse(8001, "Invalid post_id"));
                return;
            }
            
            if (!parsePositiveInt64(routingParams[0], postId)) {
                callback(makeJsonResponse(8001, "Invalid post_id"));
                return;
            }

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(4301, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                // 验证帖子是否存在
                auto postRows = g_db->execSqlSync("SELECT id FROM posts WHERE id = ?", postId);
                if (postRows.empty()) {
                    callback(makeJsonResponse(8002, "Post not found"));
                    return;
                }

                // 获取评论总数
                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM comments WHERE post_id = ?",
                    postId
                );
                int64_t totalCount = 0;
                if (!countRows.empty()) {
                    totalCount = countRows[0]["total_count"].as<int64_t>();
                }

                // 获取评论列表(按时间正序)
                auto rows = g_db->execSqlSync(
                    "SELECT c.id, c.post_id, c.user_id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "c.content, DATE_FORMAT(c.created_at, '%Y-%m-%d %H:%i:%s') AS created_at "
                    "FROM comments c "
                    "JOIN users u ON u.id = c.user_id "
                    "WHERE c.post_id = ? "
                    "ORDER BY c.created_at ASC "
                    "LIMIT ? OFFSET ?",
                    postId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    Json::Value comment;
                    comment["comment_id"] = static_cast<Json::Int64>(row["id"].as<int64_t>());
                    comment["post_id"] = static_cast<Json::Int64>(row["post_id"].as<int64_t>());
                    comment["user_id"] = static_cast<Json::Int64>(row["user_id"].as<int64_t>());
                    comment["username"] = row["username"].as<std::string>();
                    comment["nickname"] = row["nickname"].as<std::string>();
                    comment["avatar_url"] = row["avatar_url"].as<std::string>();
                    comment["content"] = row["content"].as<std::string>();
                    comment["created_at"] = row["created_at"].as<std::string>();
                    comment["is_owner"] = (row["user_id"].as<int64_t>() == currentUserId);
                    comment["can_delete"] = (row["user_id"].as<int64_t>() == currentUserId);
                    list.append(comment);
                }

                Json::Value data = makePaginationData(
                    currentUserId,
                    page,
                    pageSize,
                    totalCount,
                    "comments",
                    list
                );

                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error comments list: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/follows/followers",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int64_t userId = 0;
            if (!parsePositiveInt64(req->getParameter("user_id"), userId)) {
                callback(makeJsonResponse(11101, "Invalid user_id"));
                return;
            }

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(11104, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto userRows = g_db->execSqlSync("SELECT id FROM users WHERE id = ?", userId);
                if (userRows.empty()) {
                    callback(makeJsonResponse(11107, "User not found"));
                    return;
                }

                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM user_follows WHERE following_id = ?",
                    userId
                );
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto rows = g_db->execSqlSync(
                    "SELECT u.id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "COALESCE(u.bio, '') AS bio, "
                    "DATE_FORMAT(f.created_at, '%Y-%m-%d %H:%i:%s') AS followed_at, "
                    "CASE WHEN f_me.id IS NULL THEN 0 ELSE 1 END AS followed_by_me "
                    "FROM user_follows f "
                    "JOIN users u ON u.id = f.follower_id "
                    "LEFT JOIN user_follows f_me ON f_me.follower_id = ? AND f_me.following_id = u.id "
                    "WHERE f.following_id = ? ORDER BY f.id DESC LIMIT ? OFFSET ?",
                    currentUserId,
                    userId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToUserCardJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "followers", list);
                data["user_id"] = static_cast<Json::Int64>(userId);
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/follows/following",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int64_t userId = 0;
            if (!parsePositiveInt64(req->getParameter("user_id"), userId)) {
                callback(makeJsonResponse(11201, "Invalid user_id"));
                return;
            }

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(11204, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto userRows = g_db->execSqlSync("SELECT id FROM users WHERE id = ?", userId);
                if (userRows.empty()) {
                    callback(makeJsonResponse(11207, "User not found"));
                    return;
                }

                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM user_follows WHERE follower_id = ?",
                    userId
                );
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto rows = g_db->execSqlSync(
                    "SELECT u.id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "COALESCE(u.bio, '') AS bio, "
                    "DATE_FORMAT(f.created_at, '%Y-%m-%d %H:%i:%s') AS followed_at, "
                    "CASE WHEN f_me.id IS NULL THEN 0 ELSE 1 END AS followed_by_me "
                    "FROM user_follows f "
                    "JOIN users u ON u.id = f.following_id "
                    "LEFT JOIN user_follows f_me ON f_me.follower_id = ? AND f_me.following_id = u.id "
                    "WHERE f.follower_id = ? ORDER BY f.id DESC LIMIT ? OFFSET ?",
                    currentUserId,
                    userId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToUserCardJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "following", list);
                data["user_id"] = static_cast<Json::Int64>(userId);
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/feed/following",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;
            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(11300, errorMessage));
                return;
            }

            int page = 1;
            int pageSize = 20;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(11301, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM posts p "
                    "JOIN user_follows uf ON uf.following_id = p.user_id AND uf.follower_id = ?",
                    currentUserId
                );
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto rows = g_db->execSqlSync(
                    "SELECT p.id, p.user_id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "p.content, COALESCE(p.image_url, '') AS image_url, "
                    "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
                    "COALESCE(lc.like_count, 0) AS like_count, "
                    "COALESCE(cc.comment_count, 0) AS comment_count, "
                    "CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked "
                    "FROM posts p "
                    "JOIN user_follows uf ON uf.following_id = p.user_id AND uf.follower_id = ? "
                    "JOIN users u ON u.id = p.user_id "
                    "LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ? "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS comment_count FROM comments GROUP BY post_id) cc ON cc.post_id = p.id "
                    "ORDER BY p.id DESC LIMIT ? OFFSET ?",
                    currentUserId,
                    currentUserId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToPostJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "posts", list);
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/users/home",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            std::string userIdStr = req->getParameter("user_id");
            int64_t userId = 0;
            if (userIdStr.empty()) {
                if (currentUserId <= 0) {
                    callback(makeJsonResponse(11401, "user_id is required when not logged in"));
                    return;
                }
                userId = currentUserId;
            } else if (!parsePositiveInt64(userIdStr, userId)) {
                callback(makeJsonResponse(11402, "Invalid user_id"));
                return;
            }

            int page = 1;
            int pageSize = 10;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage, 10)) {
                callback(makeJsonResponse(11404, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto userRows = g_db->execSqlSync(
                    "SELECT id, username, COALESCE(nickname, '') AS nickname, "
                    "COALESCE(avatar_url, '') AS avatar_url, "
                    "COALESCE(bio, '') AS bio, "
                    "COALESCE(cover_image_url, '') AS cover_image_url "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (userRows.empty()) {
                    callback(makeJsonResponse(11407, "User not found"));
                    return;
                }

                bool isFollowing = false;
                if (currentUserId > 0 && currentUserId != userId) {
                    auto followRows = g_db->execSqlSync(
                        "SELECT id FROM user_follows WHERE follower_id = ? AND following_id = ?",
                        currentUserId,
                        userId
                    );
                    isFollowing = !followRows.empty();
                }

                auto countRows = g_db->execSqlSync("SELECT COUNT(*) AS total_count FROM posts WHERE user_id = ?", userId);
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto postRows = g_db->execSqlSync(
                    postListSql("WHERE p.user_id = ?"),
                    currentUserId,
                    userId,
                    pageSize,
                    offset
                );

                Json::Value posts(Json::arrayValue);
                for (const auto& row : postRows) {
                    posts.append(rowToPostJson(row, currentUserId));
                }

                Json::Value user;
                user["user_id"] = static_cast<Json::Int64>(userRows[0]["id"].as<int64_t>());
                user["username"] = userRows[0]["username"].as<std::string>();
                user["nickname"] = userRows[0]["nickname"].as<std::string>();
                user["avatar_url"] = userRows[0]["avatar_url"].as<std::string>();
                user["bio"] = userRows[0]["bio"].as<std::string>();
                user["cover_image_url"] = userRows[0]["cover_image_url"].as<std::string>();
                user["is_self"] = currentUserId > 0 && currentUserId == userId;
                user["is_following"] = isFollowing;

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "posts", posts);
                data["user"] = user;
                queryUserStatsAsync(
                    userId,
                    [callback, data](Json::Value stats) mutable {
                        data["stats"] = stats;
                        callback(makeJsonResponse(0, "OK", data));
                    },
                    [callback](const drogon::orm::DrogonDbException& e) {
                        std::cerr << "database error: " << e.base().what() << std::endl;
                        callback(makeJsonResponse(5001, "Database error"));
                    }
                );
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/users/profile/update",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(makeJsonResponse(11500, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(11501, "Request body must be JSON"));
                return;
            }

            try {
                auto userRows = g_db->execSqlSync(
                    "SELECT id, username, COALESCE(nickname, '') AS nickname, "
                    "COALESCE(avatar_url, '') AS avatar_url, "
                    "COALESCE(bio, '') AS bio, "
                    "COALESCE(cover_image_url, '') AS cover_image_url "
                    "FROM users WHERE id = ?",
                    userId
                );

                if (userRows.empty()) {
                    callback(makeJsonResponse(11502, "User not found"));
                    return;
                }

                std::string nickname = userRows[0]["nickname"].as<std::string>();
                std::string avatarUrl = userRows[0]["avatar_url"].as<std::string>();
                std::string bio = userRows[0]["bio"].as<std::string>();
                std::string coverImageUrl = userRows[0]["cover_image_url"].as<std::string>();

                if ((*json).isMember("nickname")) {
                    nickname = (*json)["nickname"].asString();
                }
                if ((*json).isMember("avatar_url")) {
                    avatarUrl = (*json)["avatar_url"].asString();
                }
                if ((*json).isMember("bio")) {
                    bio = (*json)["bio"].asString();
                }
                if ((*json).isMember("cover_image_url")) {
                    coverImageUrl = (*json)["cover_image_url"].asString();
                }

                if (nickname.length() > 60) {
                    callback(makeJsonResponse(11503, "nickname must not exceed 60 characters"));
                    return;
                }
                if (avatarUrl.length() > 500) {
                    callback(makeJsonResponse(11504, "avatar_url must not exceed 500 characters"));
                    return;
                }
                if (bio.length() > 500) {
                    callback(makeJsonResponse(11505, "bio must not exceed 500 characters"));
                    return;
                }
                if (coverImageUrl.length() > 500) {
                    callback(makeJsonResponse(11506, "cover_image_url must not exceed 500 characters"));
                    return;
                }

                g_db->execSqlSync(
                    "UPDATE users SET nickname = NULLIF(?, ''), avatar_url = NULLIF(?, ''), "
                    "bio = NULLIF(?, ''), cover_image_url = NULLIF(?, '') WHERE id = ?",
                    nickname,
                    avatarUrl,
                    bio,
                    coverImageUrl,
                    userId
                );

                auto rows = g_db->execSqlSync(
                    "SELECT id, username, COALESCE(nickname, '') AS nickname, "
                    "COALESCE(avatar_url, '') AS avatar_url, "
                    "COALESCE(bio, '') AS bio, "
                    "COALESCE(cover_image_url, '') AS cover_image_url "
                    "FROM users WHERE id = ?",
                    userId
                );

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(rows[0]["id"].as<int64_t>());
                data["username"] = rows[0]["username"].as<std::string>();
                data["nickname"] = rows[0]["nickname"].as<std::string>();
                data["avatar_url"] = rows[0]["avatar_url"].as<std::string>();
                data["bio"] = rows[0]["bio"].as<std::string>();
                data["cover_image_url"] = rows[0]["cover_image_url"].as<std::string>();

                callback(makeJsonResponse(0, "Profile updated", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/users/media",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            getOptionalUserIdFromRequest(req, currentUserId);

            int64_t userId = 0;
            if (!parsePositiveInt64(req->getParameter("user_id"), userId)) {
                callback(makeJsonResponse(11601, "Invalid user_id"));
                return;
            }

            int page = 1;
            int pageSize = 20;
            std::string errorMessage;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(11604, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto userRows = g_db->execSqlSync("SELECT id FROM users WHERE id = ?", userId);
                if (userRows.empty()) {
                    callback(makeJsonResponse(11607, "User not found"));
                    return;
                }

                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM posts WHERE user_id = ? AND image_url IS NOT NULL AND image_url <> ''",
                    userId
                );
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto rows = g_db->execSqlSync(
                    postListSql("WHERE p.user_id = ? AND p.image_url IS NOT NULL AND p.image_url <> ''"),
                    currentUserId,
                    userId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToPostJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "media_posts", list);
                data["user_id"] = static_cast<Json::Int64>(userId);
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/posts/mine",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;
            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(11700, errorMessage));
                return;
            }

            int page = 1;
            int pageSize = 20;
            if (!readPagination(req, page, pageSize, errorMessage)) {
                callback(makeJsonResponse(11701, errorMessage));
                return;
            }

            int64_t offset = static_cast<int64_t>(page - 1) * pageSize;

            try {
                auto countRows = g_db->execSqlSync(
                    "SELECT COUNT(*) AS total_count FROM posts WHERE user_id = ?",
                    currentUserId
                );
                int64_t totalCount = countRows[0]["total_count"].as<int64_t>();

                auto rows = g_db->execSqlSync(
                    postListSql("WHERE p.user_id = ?"),
                    currentUserId,
                    currentUserId,
                    pageSize,
                    offset
                );

                Json::Value list(Json::arrayValue);
                for (const auto& row : rows) {
                    list.append(rowToPostJson(row, currentUserId));
                }

                Json::Value data = makePaginationData(currentUserId, page, pageSize, totalCount, "posts", list);
                callback(makeJsonResponse(0, "OK", data));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Get}
    );

    app().registerHandler(
        "/api/posts/update",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t currentUserId = 0;
            std::string errorMessage;
            if (!getUserIdFromRequest(req, currentUserId, errorMessage)) {
                callback(makeJsonResponse(11800, errorMessage));
                return;
            }

            auto json = req->getJsonObject();
            if (!json) {
                callback(makeJsonResponse(11801, "Request body must be JSON"));
                return;
            }

            int64_t postId = 0;
            if (!parseJsonInt64(*json, "post_id", postId)) {
                callback(makeJsonResponse(11802, "Invalid post_id"));
                return;
            }

            try {
                auto postRows = g_db->execSqlSync(
                    "SELECT id, user_id, content, COALESCE(image_url, '') AS image_url FROM posts WHERE id = ?",
                    postId
                );

                if (postRows.empty()) {
                    callback(makeJsonResponse(11805, "Post not found"));
                    return;
                }

                int64_t postUserId = postRows[0]["user_id"].as<int64_t>();
                if (postUserId != currentUserId) {
                    callback(makeJsonResponse(11806, "Only the owner can update this post"));
                    return;
                }

                std::string content = postRows[0]["content"].as<std::string>();
                std::string imageUrl = postRows[0]["image_url"].as<std::string>();

                if ((*json).isMember("content")) {
                    content = (*json)["content"].asString();
                }
                if ((*json).isMember("image_url")) {
                    imageUrl = (*json)["image_url"].asString();
                }

                if (content.empty()) {
                    callback(makeJsonResponse(11807, "content is required"));
                    return;
                }
                if (content.length() > 2000) {
                    callback(makeJsonResponse(11808, "content must not exceed 2000 characters"));
                    return;
                }
                if (imageUrl.length() > 500) {
                    callback(makeJsonResponse(11809, "image_url must not exceed 500 characters"));
                    return;
                }

                g_db->execSqlSync(
                    "UPDATE posts SET content = ?, image_url = NULLIF(?, '') WHERE id = ? AND user_id = ?",
                    content,
                    imageUrl,
                    postId,
                    currentUserId
                );

                auto rows = g_db->execSqlSync(
                    "SELECT p.id, p.user_id, u.username, "
                    "COALESCE(u.nickname, '') AS nickname, "
                    "COALESCE(u.avatar_url, '') AS avatar_url, "
                    "p.content, COALESCE(p.image_url, '') AS image_url, "
                    "DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s') AS created_at, "
                    "COALESCE(lc.like_count, 0) AS like_count, "
                    "COALESCE(cc.comment_count, 0) AS comment_count, "
                    "CASE WHEN pl_me.user_id IS NULL THEN 0 ELSE 1 END AS liked "
                    "FROM posts p "
                    "JOIN users u ON u.id = p.user_id "
                    "LEFT JOIN post_likes pl_me ON pl_me.post_id = p.id AND pl_me.user_id = ? "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS like_count FROM post_likes GROUP BY post_id) lc ON lc.post_id = p.id "
                    "LEFT JOIN (SELECT post_id, COUNT(*) AS comment_count FROM comments GROUP BY post_id) cc ON cc.post_id = p.id "
                    "WHERE p.id = ?",
                    currentUserId,
                    postId
                );

                if (rows.empty()) {
                    callback(makeJsonResponse(11810, "Updated post not found"));
                    return;
                }

                callback(makeJsonResponse(0, "Post updated", rowToPostJson(rows[0], currentUserId)));
            } catch (const drogon::orm::DrogonDbException& e) {
                std::cerr << "database error: " << e.base().what() << std::endl;
                callback(makeJsonResponse(5001, "Database error"));
            }
        },
        {Post}
    );

    app()
        .setClientMaxBodySize(60 * 1024 * 1024)
        .setUploadPath("./uploads")
        .addListener("0.0.0.0", 8080)
        .setThreadNum(4)
        .run();

    return 0;
}