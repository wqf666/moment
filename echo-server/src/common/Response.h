#pragma once

#include <string>
#include <json/json.h>
#include <drogon/drogon.h>

drogon::HttpResponsePtr makeJsonResponse(
    int code,
    const std::string& message,
    const Json::Value& data = Json::Value(Json::objectValue)
);