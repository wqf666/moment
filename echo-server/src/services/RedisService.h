#pragma once

#include <sw/redis++/redis++.h>
#include <optional>
#include <string>

class RedisService {
public:
    static RedisService& instance();

    void set(
        const std::string& key,
        const std::string& value,
        int ttl = 0
    );

    std::optional<std::string> get(
        const std::string& key
    );

    bool exists(const std::string& key);

    long long incr(const std::string& key);

private:
    RedisService();

    sw::redis::Redis redis_;
};