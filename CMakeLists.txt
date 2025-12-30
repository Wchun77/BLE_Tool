cmake_minimum_required(VERSION 3.20)
project(BLE_Tool VERSION 1.0.0 LANGUAGES CXX)

# C++ 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 特定設定
if(MSVC)
    # 啟用多處理器編譯
    add_compile_options(/MP)
    
    # 警告等級
    add_compile_options(/W3)
    
    # UTF-8 編碼支援
    add_compile_options(/utf-8)
    
    # Unicode 支援
    add_compile_definitions(UNICODE _UNICODE)
    
    # C++/WinRT 需要的定義
    add_compile_definitions(
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    
    # 避免 Windows.h 的 min/max 巨集
    add_compile_definitions(NOMINMAX)
endif()

# 輸出目錄
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 平台檢測
if(WIN32)
    set(PLATFORM_NAME "windows")
    add_definitions(-DPLATFORM_WINDOWS)
else()
    message(FATAL_ERROR "Currently only Windows is supported")
endif()

message(STATUS "Building for platform: ${PLATFORM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 子專案
add_subdirectory(core)
add_subdirectory(platform/${PLATFORM_NAME})
add_subdirectory(lib)
add_subdirectory(app)
add_subdirectory(tests)