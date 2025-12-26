# 核心業務邏輯層
add_library(ble_core STATIC
    src/BLEManager.cpp
    src/BLEDevice.cpp
)

target_include_directories(ble_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# 依賴平台層
target_link_libraries(ble_core
    PUBLIC
        ble_platform
)

target_compile_features(ble_core PUBLIC cxx_std_17)