#include "routes/RegisterRoutes.h"

#include "controllers/SystemController.h"
#include "controllers/MediaController.h"
#include "controllers/AuthController.h"
#include "controllers/AiController.h"
#include "controllers/PostController.h"

namespace routes {

void registerRoutes() {
    controllers::registerSystemRoutes();
    controllers::registerMediaRoutes();
    controllers::registerAuthRoutes();
    controllers::registerAiRoutes();
    controllers::registerPostRoutes();
}

}
