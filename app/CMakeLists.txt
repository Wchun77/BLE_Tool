# C++ 測試應用程式
add_executable(ble_tool
    main.cpp
)

target_link_libraries(ble_tool
    PRIVATE
        ble_core
        ble_platform
)

target_compile_features(ble_tool PRIVATE cxx_std_17)