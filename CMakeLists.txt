cmake_minimum_required(VERSION 3.16)
project(cpp_passman)

set(CMAKE_CXX_STANDARD 17)

set(MIN_SODIUM_VERSION "1.0.18")
set(MIN_OQS_VERSION    "0.10.0")

include_directories(
        "C:/msys64/mingw64/include/wx-3.2"
        "C:/msys64/mingw64/lib/wx/include/msw-unicode-3.2"
)
link_directories("C:/msys64/mingw64/lib")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM_PC QUIET libsodium>=${MIN_SODIUM_VERSION})
    if(SODIUM_PC_FOUND)
        message(STATUS "libsodium ${SODIUM_PC_VERSION} found via pkg-config ✓")
    endif()
endif()

find_library(SODIUM_LIB sodium
        PATHS "C:/msys64/mingw64/lib" "$ENV{MSYS2_ROOT}/mingw64/lib")
if(NOT SODIUM_LIB)
    message(FATAL_ERROR
            "libsodium >= ${MIN_SODIUM_VERSION} not found.\n"
            "Install via MSYS2 MINGW64 shell:\n"
            "  pacman -S mingw-w64-x86_64-libsodium\n"
            "Then re-run CMake.")
endif()
message(STATUS "libsodium: ${SODIUM_LIB}")

option(OQS_FETCH
        "Download and build liboqs automatically if not found on the system" OFF)

find_library(OQS_LIB oqs
        PATHS
        "C:/msys64/mingw64/lib"
        "$ENV{MSYS2_ROOT}/mingw64/lib"
        "$ENV{OQS_ROOT}/lib"
        NO_DEFAULT_PATH)

if(NOT OQS_LIB)
    find_library(OQS_LIB oqs)
endif()

find_path(OQS_INCLUDE_DIR oqs/oqs.h
        PATHS
        "C:/msys64/mingw64/include"
        "$ENV{MSYS2_ROOT}/mingw64/include"
        "$ENV{OQS_ROOT}/include"
        NO_DEFAULT_PATH)

if(NOT OQS_INCLUDE_DIR)
    find_path(OQS_INCLUDE_DIR oqs/oqs.h)
endif()

if(OQS_LIB AND OQS_INCLUDE_DIR)
    message(STATUS "liboqs: ${OQS_LIB}  (headers: ${OQS_INCLUDE_DIR})")
    include_directories(${OQS_INCLUDE_DIR})

elseif(OQS_FETCH)
    message(STATUS "liboqs not found — downloading via FetchContent (OQS_FETCH=ON)")
    include(FetchContent)
    FetchContent_Declare(
            liboqs
            GIT_REPOSITORY https://github.com/open-quantum-safe/liboqs.git
            GIT_TAG        main
            GIT_SHALLOW    TRUE
    )
    set(OQS_ALGS_ENABLED "ML-KEM" CACHE STRING "" FORCE)
    set(OQS_BUILD_ONLY_LIB ON  CACHE BOOL "" FORCE)
    set(OQS_USE_OPENSSL    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(liboqs)

    set(OQS_LIB oqs)
    set(OQS_FETCHED TRUE)
    message(STATUS "liboqs will be built from source ✓")

else()
    message(FATAL_ERROR
            "liboqs >= ${MIN_OQS_VERSION} not found.\n"
            "\n"
            "Option A — Install via MSYS2 MINGW64 shell (recommended):\n"
            "  pacman -S mingw-w64-x86_64-liboqs\n"
            "  Then re-run CMake.\n"
            "\n"
            "Option B — Let CMake download + build liboqs automatically:\n"
            "  Add -DOQS_FETCH=ON to your CMake configuration.\n"
            "  In CLion: Settings > Build > CMake > CMake options: -DOQS_FETCH=ON\n"
            "\n"
            "Option C — Install liboqs manually and point CMake to it:\n"
            "  cmake -DOQS_ROOT=C:/path/to/liboqs/install .\n")
endif()

add_executable(cpp_passman
        main.cpp
        SecureMemory.h
        AuditLog.h
        SecurePasswordDialog.h
        PasswordDatabase.cpp
        PasswordDatabase.h
        EntryUI/NewEntryDialog.cpp
        EntryUI/NewEntryDialog.h
        EntryUI/EditEntryDialog.cpp
        EntryUI/EditEntryDialog.h
        PasswordGeneratorDialog.cpp
        PasswordGeneratorDialog.h
)

target_link_libraries(cpp_passman
        -Wl,--start-group
        wx_mswu_adv-3.2
        wx_mswu_core-3.2
        wx_baseu-3.2
        wxexpat-3.2
        wxjpeg-3.2
        wxpng-3.2
        wxtiff-3.2
        wxzlib-3.2
        -Wl,--end-group
        gdi32 comdlg32 winspool winmm shell32 ole32 oleaut32 uuid odbc32 odbccp32
        uxtheme comctl32 shlwapi version oleacc
        wtsapi32
        advapi32
        ${SODIUM_LIB}
        ${OQS_LIB}
)

if(DEFINED OQS_FETCHED)
    target_link_libraries(cpp_passman oqs)
endif()

target_link_options(cpp_passman PRIVATE -static -static-libgcc -static-libstdc++)

add_executable(passman_tests
        tests/test_database.cpp
        PasswordDatabase.cpp
        PasswordDatabase.h
        SecureMemory.h
        AuditLog.h
)
target_include_directories(passman_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(passman_tests ${SODIUM_LIB} ${OQS_LIB})
if(DEFINED OQS_FETCHED)
    target_link_libraries(passman_tests oqs)
endif()

enable_testing()
add_test(NAME PassManUnitTests COMMAND passman_tests)

option(FUZZ "Build libFuzzer harnesses (requires clang -fsanitize=fuzzer)" OFF)
if(FUZZ)
    foreach(FTARGET fuzz_deserialize fuzz_fileformat)
        add_executable(${FTARGET}
                fuzz/${FTARGET}.cpp
                PasswordDatabase.cpp
        )
        target_compile_options(${FTARGET} PRIVATE
                -fsanitize=fuzzer,address,undefined -DFUZZ_FAST_KDF)
        target_link_options(${FTARGET} PRIVATE
                -fsanitize=fuzzer,address,undefined)
        target_link_libraries(${FTARGET} ${SODIUM_LIB} ${OQS_LIB})
        target_include_directories(${FTARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    endforeach()
endif()

message(STATUS "")
message(STATUS "PassMan — security configuration")
message(STATUS "  PQC KEM    : ML-KEM-768 (FIPS 203) via liboqs >= ${MIN_OQS_VERSION}")
message(STATUS "  KDF        : Argon2id 512 MB / 4 ops")
message(STATUS "  AEAD       : XChaCha20-Poly1305 with full header AAD")
message(STATUS "  Format     : PMDB v4")
message(STATUS "  Perms      : 0600 / user-only DACL")
message(STATUS "  Unit tests : cmake --build . --target passman_tests")
message(STATUS "  Fuzzing    : cmake -DFUZZ=ON (requires clang)")
message(STATUS "")
