#include "common/Auth.h"

bool getUserIdFromRequest(
    const drogon::HttpRequestPtr& req,
    int64_t& userId,
    std::string& errorMessage
) {
    std::string auth = req->getHeader("Authorization");

    if (auth.empty()) {
        auth = req->getHeader("authorization");
    }

    const std::string bearerPrefix = "Bearer ";
    const std::string tokenPrefix = "demo_token_";

    if (auth.empty()) {
        errorMessage = "Missing Authorization header";
        return false;
    }

    if (auth.rfind(bearerPrefix, 0) != 0) {
        errorMessage = "Invalid Authorization format";
        return false;
    }

    std::string token = auth.substr(bearerPrefix.size());

    if (token.rfind(tokenPrefix, 0) != 0) {
        errorMessage = "Invalid token";
        return false;
    }

    std::string idText = token.substr(tokenPrefix.size());

    try {
        size_t pos = 0;
        long long parsed = std::stoll(idText, &pos);

        if (pos != idText.size() || parsed <= 0) {
            errorMessage = "Invalid token user id";
            return false;
        }

        userId = static_cast<int64_t>(parsed);
        return true;
    } catch (...) {
        errorMessage = "Invalid token";
        return false;
    }
}
