#include "common/MediaUtil.h"

#include <algorithm>
#include <cctype>

bool isSafeMediaFileName(const std::string& fileName) {
    if (fileName.empty()) {
        return false;
    }

    if (
        fileName.find("..") != std::string::npos ||
        fileName.find('/') != std::string::npos ||
        fileName.find('\\') != std::string::npos
    ) {
        return false;
    }

    return std::all_of(
        fileName.begin(),
        fileName.end(),
        [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        }
    );
}
