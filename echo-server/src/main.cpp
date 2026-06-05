#include <drogon/drogon.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <string>
#include "common/AppContext.h"
#include "routes/RegisterRoutes.h"

int main() {
    using namespace drogon;

    try {
        appctx::initAppContext();

        std::filesystem::create_directories("./uploads");

        routes::registerRoutes();

        app().addListener("127.0.0.1", 8080);

        std::cout << "echo-server started at http://127.0.0.1:8080" << std::endl;

        app().run();
    } catch (const std::exception& e) {
        std::cerr << "server startup failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}