
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" {
#include <sodium.h>
}
#include "../PasswordDatabase.h"

static bool g_init = []() -> bool {
    sodium_init();
    OQS_init();
    return true;
}();

static size_t g_iter = 0;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 6 || size > 1024 * 1024) return 0;

    const std::string path =
        std::filesystem::temp_directory_path().string() +
        "/passman_fuzz_" + std::to_string(g_iter++) + ".pmdb";

    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return 0;
        f.write(reinterpret_cast<const char*>(data), size);
    }

    try {
        PasswordDatabase db(path);
        db.unlock("fuzz_password");
    } catch (const std::exception&) {
    } catch (...) {
        std::filesystem::remove(path);
        std::filesystem::remove(path + ".audit.log");
        __builtin_trap();
    }

    std::filesystem::remove(path);
    std::filesystem::remove(path + ".audit.log");
    return 0;
}
