# C++ 測試應用程式
add_executable(ble_tool
    main.cpp
    blue_bridge_helper.cpp
)

target_include_directories(ble_tool
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(ble_tool
    PRIVATE
        ble_core
        ble_platform
)

target_compile_features(ble_tool PRIVATE cxx_std_17)