#pragma once

#include <atomic>
#include <cstdint>

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/nosql/RedisClient.h>

namespace appctx {

extern drogon::orm::DbClientPtr db;
extern drogon::HttpClientPtr aiClient;
extern drogon::nosql::RedisClientPtr redis;
extern std::atomic<uint64_t> uploadSequence;

void initAppContext();

}