# DLL 導出層（C API）
add_library(ble_api SHARED
    src/ble_api.cpp
)

target_include_directories(ble_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(ble_api
    PRIVATE
        ble_core
        ble_platform
)

# DLL 導出設定
target_compile_definitions(ble_api PRIVATE BLE_API_EXPORTS)

# 輸出名稱
set_target_properties(ble_api PROPERTIES
    OUTPUT_NAME "ble_api"
    PREFIX ""
)

if(WIN32)
    # Windows DLL 特定設定
    set_target_properties(ble_api PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )
endif()