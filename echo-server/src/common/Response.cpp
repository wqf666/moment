#include "common/Response.h"

drogon::HttpResponsePtr makeJsonResponse(
    int code,
    const std::string& message,
    const Json::Value& data
) {
    Json::Value root;
    root["code"] = code;
    root["message"] = message;

    if (data.isNull()) {
        root["data"] = Json::Value(Json::objectValue);
    } else {
        root["data"] = data;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}