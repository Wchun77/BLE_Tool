#pragma once

#include "IBLEBackend.h"
#include <string>
#include <map>
#include <mutex>
#include <memory>

namespace BLE {

class WindowsBLE : public IBLEBackend {
public:
    WindowsBLE();
    ~WindowsBLE() override;

    // IBLEBackend 實作
    bool initialize() override;
    void shutdown() override;

    bool startScan(
        int timeoutMs,
        ScanMode mode,
        ScanFoundCallback foundCallback,
        ScanUpdatedCallback updatedCallback) override;
    void stopScan() override;

    bool connect(const std::string& deviceAddress) override;
    bool disconnect() override;
    bool isConnected() const override;
    void setConnectionCallback(ConnectionCallback callback) override;

    void setPairingPin(const std::string& pin) override;
    int isPaired(const std::string& deviceAddress) override;
    bool unpair(const std::string& deviceAddress) override;

    bool discoverServices() override;
    std::vector<std::string> getServices() override;
    std::vector<std::string> getCharacteristics(const std::string& serviceUuid) override;

    bool writeData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        const uint8_t* data,
        size_t length,
        bool withResponse) override;

    bool readData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        std::vector<uint8_t>& outData) override;

    bool subscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid,
        DataCallback callback) override;

    bool unsubscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid) override;

    std::string getLastError() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
    
    mutable std::mutex mutex_;
    std::string lastError_;
    
    ScanFoundCallback scanFoundCallback_;
    ScanUpdatedCallback scanUpdatedCallback_;
    
    ConnectionCallback connectionCallback_;
    std::string pairingPin_;    // 空字串 = 用系統配對 (原本的行為)
    std::map<std::string, DataCallback> notifyCallbacks_;

    void setError(const std::string& error);
    
    // 輔助函數
    uint64_t addressStringToUint64(const std::string& address);
    DeviceInfo extractDeviceInfo(const std::string& address, const struct AdvPair& pair);
};

} // namespace BLE