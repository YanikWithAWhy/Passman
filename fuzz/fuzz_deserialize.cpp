
#include <cstdint>
#include <cstring>
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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 16384) return 0;

    const std::string line(reinterpret_cast<const char*>(data), size);

    PasswordEntry e;
    bool ok = e.deserialize(line);

    if (ok) {
        std::string serialized = e.serialize();
        if (!serialized.empty() && serialized.back() == '\n')
            serialized.pop_back();

        PasswordEntry e2;
        bool ok2 = e2.deserialize(serialized);
        if (!ok2) __builtin_trap();
    }

    return 0;
}
