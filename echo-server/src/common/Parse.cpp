#include "common/Parse.h"

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

bool parseJsonInt64(
    const Json::Value& json,
    const std::string& key,
    int64_t& out
) {
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

bool parseOptionalJsonInt64(
    const Json::Value& json,
    const std::string& key,
    int64_t& out
) {
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