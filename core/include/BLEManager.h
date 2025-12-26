#pragma once

#include "BLETypes.h"
#include "IBLEBackend.h"
#include <memory>
#include <string>
#include <vector>

namespace BLE {

class BLEManager {
public:
    BLEManager();
    ~BLEManager();

    // 禁止複製
    BLEManager(const BLEManager&) = delete;
    BLEManager& operator=(const BLEManager&) = delete;

    // 初始化
    bool initialize();
    void shutdown();
    bool isInitialized() const;

    // 掃描
    bool startScan(int timeoutMs = 5000);
    void stopScan();
    void setScanCallback(ScanCallback callback);
    std::vector<DeviceInfo> getScannedDevices() const;

    // 連線
    bool connect(const std::string& deviceAddress);
    bool disconnect();
    bool isConnected() const;
    void setConnectionCallback(ConnectionCallback callback);

    // 服務/特性
    bool discoverServices();
    std::vector<std::string> getServices();
    std::vector<std::string> getCharacteristics(const std::string& serviceUuid);

    // 資料操作
    bool writeData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        const uint8_t* data,
        size_t length,
        bool withResponse = true);

    bool readData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        std::vector<uint8_t>& outData);

    // 通知
    bool subscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid,
        DataCallback callback);

    bool unsubscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid);

    // 工具函數
    std::string getLastError() const;
    void setErrorCallback(ErrorCallback callback);

private:
    std::unique_ptr<IBLEBackend> backend_;
    std::vector<DeviceInfo> scannedDevices_;
    ScanCallback scanCallback_;
    ErrorCallback errorCallback_;
    bool initialized_;
    
    void handleError(const std::string& error);
};

} // namespace BLE