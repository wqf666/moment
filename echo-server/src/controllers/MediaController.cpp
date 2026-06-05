#include "controllers/MediaController.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

#include <json/json.h>
#include <drogon/drogon.h>

#include "common/AppContext.h"
#include "common/Auth.h"
#include "common/MediaUtil.h"
#include "common/Response.h"

namespace controllers {

namespace {

drogon::HttpResponsePtr jsonResp(
    int code,
    const std::string& message,
    const Json::Value& data = Json::Value(Json::objectValue)
) {
    return makeJsonResponse(code, message, data);
}

std::string getFileExtFromName(const std::string& fileName) {
    auto pos = fileName.find_last_of('.');

    if (pos == std::string::npos || pos + 1 >= fileName.size()) {
        return ".bin";
    }

    std::string ext = fileName.substr(pos);

    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (
        ext == ".jpg" ||
        ext == ".jpeg" ||
        ext == ".png" ||
        ext == ".gif" ||
        ext == ".webp" ||
        ext == ".mp4" ||
        ext == ".mov" ||
        ext == ".avi"
    ) {
        return ext;
    }

    return ".bin";
}

std::string buildUploadFileName(const std::string& originalName) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    uint64_t seq = ++appctx::uploadSequence;

    return std::to_string(millis) + "_" + std::to_string(seq) + getFileExtFromName(originalName);
}

} // namespace

void registerMediaRoutes() {
    using namespace drogon;

    std::filesystem::create_directories("./uploads");

    app().registerHandler(
        "/api/upload/media",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            int64_t userId = 0;
            std::string errorMessage;

            if (!getUserIdFromRequest(req, userId, errorMessage)) {
                callback(jsonResp(14000, errorMessage));
                return;
            }

            MultiPartParser parser;

            if (parser.parse(req) != 0) {
                callback(jsonResp(14001, "Invalid multipart request"));
                return;
            }

            auto files = parser.getFiles();

            if (files.empty()) {
                callback(jsonResp(14002, "No file uploaded"));
                return;
            }

            const auto& file = files[0];
            std::string originalName = file.getFileName();

            if (originalName.empty()) {
                originalName = "upload.bin";
            }

            if (!isSafeMediaFileName(originalName)) {
                originalName = "upload" + getFileExtFromName(originalName);
            }

            std::string savedName = buildUploadFileName(originalName);
            std::string savePath = "./uploads/" + savedName;

            try {
                file.saveAs(savePath);

                Json::Value data;
                data["user_id"] = static_cast<Json::Int64>(userId);
                data["file_name"] = savedName;
                data["original_name"] = originalName;
                data["url"] = "/api/media/file/" + savedName;

                callback(jsonResp(0, "Upload succeeded", data));
            } catch (const std::exception& e) {
                std::cerr << "save upload file error: " << e.what() << std::endl;
                callback(jsonResp(14003, "Save file failed"));
            }
        },
        {Post}
    );

    app().registerHandler(
        "/api/media/file/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& fileName) {
            (void)req;

            if (!isSafeMediaFileName(fileName)) {
                callback(jsonResp(14004, "Invalid file name"));
                return;
            }

            std::string path = "./uploads/" + fileName;

            if (!std::filesystem::exists(path)) {
                callback(jsonResp(14005, "File not found"));
                return;
            }

            auto resp = HttpResponse::newFileResponse(path);

            std::string ext = getFileExtFromName(fileName);

            if (ext == ".jpg" || ext == ".jpeg") {
                resp->setContentTypeCode(CT_IMAGE_JPG);
            } else if (ext == ".png") {
                resp->setContentTypeCode(CT_IMAGE_PNG);
            } else if (ext == ".gif") {
                resp->setContentTypeString("image/gif");
            } else if (ext == ".webp") {
                resp->setContentTypeString("image/webp");
            } else if (ext == ".mp4") {
                resp->setContentTypeString("video/mp4");
            } else {
                resp->setContentTypeString("application/octet-stream");
            }

            callback(resp);
        },
        {Get}
    );
}

} // namespace controllers