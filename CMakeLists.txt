cmake_minimum_required(VERSION 3.16)
project(cpp_passman)

set(CMAKE_CXX_STANDARD 17)

include_directories(
        "C:/msys64/mingw64/include/wx-3.2"
        "C:/msys64/mingw64/lib/wx/include/msw-unicode-3.2"
)

link_directories("C:/msys64/mingw64/lib")

find_library(SODIUM_LIB sodium PATHS "C:/msys64/mingw64/lib")
if (NOT SODIUM_LIB)
    message(FATAL_ERROR "libsodium not found!")
endif ()

add_executable(cpp_passman main.cpp
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
        wx_baseu-3.2
        wx_mswu_core-3.2
        wx_mswu_adv-3.2
        wxexpat-3.2
        wxjpeg-3.2
        wxpng-3.2
        wxtiff-3.2
        wxzlib-3.2
        gdi32 comdlg32 winspool winmm shell32 ole32 oleaut32 uuid odbc32 odbccp32
        ${SODIUM_LIB}
)
