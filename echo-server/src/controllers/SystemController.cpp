#include "controllers/SystemController.h"

#include <iostream>
#include <json/json.h>
#include <drogon/drogon.h>

#include "common/AppContext.h"
#include "common/Response.h"

namespace controllers {

void registerSystemRoutes() {
    using namespace drogon;

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

            appctx::redis->execCommandAsync(
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
}

}
