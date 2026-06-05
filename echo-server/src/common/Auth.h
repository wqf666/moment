#pragma once

#include <cstdint>
#include <string>
#include <drogon/drogon.h>

bool getUserIdFromRequest(
    const drogon::HttpRequestPtr& req,
    int64_t& userId,
    std::string& errorMessage
);
