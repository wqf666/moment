#include "RedisService.h"

using namespace sw::redis;

RedisService::RedisService()
    : redis_("tcp://127.0.0.1:6379")
{
}

RedisService& RedisService::instance() {
    static RedisService instance;
    return instance;
}

void RedisService::set(
    const std::string& key,
    const std::string& value,
    int ttl
) {
    redis_.set(key, value);

    if (ttl > 0) {
        redis_.expire(key, ttl);
    }
}

std::optional<std::string> RedisService::get(
    const std::string& key
) {
    return redis_.get(key);
}

bool RedisService::exists(const std::string& key) {
    return redis_.exists(key);
}

long long RedisService::incr(
    const std::string& key
) {
    return redis_.incr(key);
}