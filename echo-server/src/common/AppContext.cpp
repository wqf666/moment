#include "common/AppContext.h"

namespace appctx {

drogon::orm::DbClientPtr db;
drogon::HttpClientPtr aiClient;
drogon::nosql::RedisClientPtr redis;
std::atomic<uint64_t> uploadSequence{0};

void initAppContext() {
    db = drogon::orm::DbClient::newMysqlClient(
        "host=127.0.0.1 port=3306 dbname=echo_app user=echo_user password=123456",
        4
    );

    aiClient = drogon::HttpClient::newHttpClient("http://127.0.0.1:18080");

    redis = drogon::nosql::RedisClient::newRedisClient(
        trantor::InetAddress("127.0.0.1", 6379)
    );
}

}