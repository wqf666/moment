#pragma once

#include <cstdint>
#include <string>
#include <json/json.h>

bool parsePositiveInt64(const std::string& value, int64_t& out);

bool parseJsonInt64(
    const Json::Value& json,
    const std::string& key,
    int64_t& out
);

bool parseOptionalJsonInt64(
    const Json::Value& json,
    const std::string& key,
    int64_t& out
);
