cmake_minimum_required(VERSION 3.16)
project(cpp_passman)

set(CMAKE_CXX_STANDARD 17)

include_directories(
        "C:/msys64/mingw64/include/wx-3.2"
        "C:/msys64/mingw64/lib/wx/include/msw-unicode-3.2"
)

link_directories("C:/msys64/mingw64/lib")

find_file(SODIUM_STATIC_LIB
        NAMES libsodium.a
        PATHS "C:/msys64/mingw64/lib"
        NO_DEFAULT_PATH
)

if (NOT SODIUM_STATIC_LIB)
    message(FATAL_ERROR
            "cannot find libsodium.a\n"
            "please install using: pacman -S mingw-w64-x86_64-libsodium"
    )
endif ()

message(STATUS "using static libsodium: ${SODIUM_STATIC_LIB}")

add_executable(cpp_passman
        main.cpp
        PasswordDatabase.cpp
        PasswordDatabase.h
        EntryUI/NewEntryDialog.cpp
        EntryUI/NewEntryDialog.h
        EntryUI/EditEntryDialog.cpp
        EntryUI/EditEntryDialog.h
        PasswordGeneratorDialog.cpp
        PasswordGeneratorDialog.h
)

target_link_options(cpp_passman PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
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

        gdi32 comdlg32 winspool winmm shell32 ole32 oleaut32
        uuid odbc32 odbccp32 uxtheme comctl32 shlwapi version oleacc

        ${SODIUM_STATIC_LIB}
)