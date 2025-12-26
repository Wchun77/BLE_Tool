# Windows 平台實作層
add_library(ble_platform STATIC
    src/WindowsBLE.cpp
)

target_include_directories(ble_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
)

# Windows 特定連結庫
target_link_libraries(ble_platform
    PUBLIC
        windowsapp
        runtimeobject
)

target_compile_features(ble_platform PUBLIC cxx_std_17)

# MinGW64 需要額外設定
if(MINGW)
    target_compile_definitions(ble_platform PRIVATE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
endif()