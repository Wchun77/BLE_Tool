# BLE 掃描測試程式
add_executable(test_scan
    test_scan.cpp
)

target_include_directories(test_scan
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(test_scan
    PRIVATE
        ble_core
        ble_platform
)

target_compile_features(test_scan PRIVATE cxx_std_17)

# 設定輸出目錄
set_target_properties(test_scan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)