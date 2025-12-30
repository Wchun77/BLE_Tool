#pragma once

#include "BLETypes.h"
#include <memory>

namespace BLE {

// 平台後端介面（抽象基類）
class IBLEBackend {
public:
    virtual ~IBLEBackend() = default;

    // 初始化/清理
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    // 掃描（修改）
    virtual bool startScan(
        int timeoutMs,
        ScanMode mode,
        ScanFoundCallback foundCallback,
        ScanUpdatedCallback updatedCallback) = 0;
    virtual void stopScan() = 0;

    // 連線
    virtual bool connect(const std::string& deviceAddress) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;

    // 服務發現
    virtual bool discoverServices() = 0;
    virtual std::vector<std::string> getServices() = 0;
    virtual std::vector<std::string> getCharacteristics(const std::string& serviceUuid) = 0;

    // 資料讀寫
    virtual bool writeData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        const uint8_t* data,
        size_t length,
        bool withResponse = true) = 0;

    virtual bool readData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        std::vector<uint8_t>& outData) = 0;

    // 通知訂閱
    virtual bool subscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid,
        DataCallback callback) = 0;

    virtual bool unsubscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid) = 0;

    // 錯誤處理
    virtual std::string getLastError() const = 0;
};

// 工廠函數（由平台層實作）
std::unique_ptr<IBLEBackend> createPlatformBackend();

} // namespace BLE