#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace BLE {

// 裝置資訊（增強版）
struct DeviceInfo {
    std::string name;
    std::string address;
    int16_t rssi;
    
    // 新增欄位
    uint16_t manufacturerId;        // 廠商 ID (0 表示無)
    std::vector<std::string> serviceUuids;  // 服務 UUID 列表
    bool hasName;                   // 是否有廣播名稱
    
    DeviceInfo() 
        : rssi(0)
        , manufacturerId(0)
        , hasName(false)
    {}
    
    DeviceInfo(const std::string& n, const std::string& a, int16_t r)
        : name(n)
        , address(a)
        , rssi(r)
        , manufacturerId(0)
        , hasName(!n.empty() && n != "Unknown Device")
    {}
};

// 服務/特性 UUID
struct ServiceCharacteristic {
    std::string serviceUuid;
    std::string characteristicUuid;
    
    ServiceCharacteristic() = default;
    ServiceCharacteristic(const std::string& svc, const std::string& ch)
        : serviceUuid(svc), characteristicUuid(ch) {}
};

// 特性屬性
enum class CharacteristicProperty : uint32_t {
    None = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    WriteWithoutResponse = 1 << 2,
    Notify = 1 << 3,
    Indicate = 1 << 4,
};

// 回調函數類型
using ScanCallback = std::function<void(const DeviceInfo&)>;
using DataCallback = std::function<void(const uint8_t*, size_t)>;
using ConnectionCallback = std::function<void(bool connected, const std::string& message)>;
using ErrorCallback = std::function<void(const std::string& error)>;

// 錯誤碼
enum class BLEError {
    Success = 0,
    NotInitialized,
    AlreadyConnected,
    NotConnected,
    DeviceNotFound,
    ServiceNotFound,
    CharacteristicNotFound,
    WriteError,
    ReadError,
    SubscribeError,
    PlatformError,
    Timeout,
    InvalidParameter
};

} // namespace BLE