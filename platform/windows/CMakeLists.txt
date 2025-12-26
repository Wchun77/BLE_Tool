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
if(MSVC)
    target_link_libraries(ble_platform
        PUBLIC
            windowsapp
            runtimeobject
    )
    
    # 確保使用 C++/WinRT
    target_compile_options(ble_platform PRIVATE /await)
endif()

target_compile_features(ble_platform PUBLIC cxx_std_17)