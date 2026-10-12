#include "resources.h"
#include <climits>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#ifndef CHITON_INSTALL_DIR
#define CHITON_INSTALL_DIR "/usr/local/chiton/current"
#endif
namespace resources {
namespace {
bool readable(const std::string &path) {
    struct stat s{};
    return stat(path.c_str(), &s) == 0 && S_ISREG(s.st_mode) && access(path.c_str(), R_OK) == 0;
}
}
std::string executable_directory() {
    char path[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0) return ".";
    path[n] = '\0';
    std::string result(path);
    return result.substr(0, result.find_last_of('/'));
}
std::string find(const std::string &relative) {
    // Versioned bundles must not silently read assets/plugins from an older
    // compiled-in current release during an unactivated smoke test.
    if (const char *root = getenv("CHITON_RESOURCE_DIR")) {
        if (*root) {
            const std::string path = std::string(root) + "/" + relative;
            return readable(path) ? path : "";
        }
    }
    const std::string exe = executable_directory();
    for (const auto &root : std::vector<std::string>{CHITON_INSTALL_DIR, exe, exe + "/../release", exe + "/../../release", "."}) {
        std::string path = root + "/" + relative;
        if (readable(path)) return path;
    }
    return "";
}
std::string asset(const std::string &filename) {
    if (const char *dir = getenv("CHITON_ASSET_DIR")) {
        // An explicit override also allows testing missing-asset behavior.
        std::string path = std::string(dir) + "/" + filename;
        return readable(path) ? path : "";
    }
    auto path = find("files/" + filename);
    return path.empty() ? find(filename) : path;
}
}
