#pragma once

#include "BLEManager.h"
#include <string>
#include <functional>

namespace BlueBridge {

// Blue Bridge 配置常數
const std::string DEVICE_NAME = "Blue Bridge";
const std::string SERVICE_UUID = "0000abf0-0000-1000-8000-00805f9b34fb";
const std::string CHAR_UUID = "0000abf1-0000-1000-8000-00805f9b34fb";
const std::string PIN_CODE = "000000";

// 命令包裝器
class CommandWrapper {
public:
    // 包裝命令（加上 \r\n...\r\n）
    static std::string wrap(const std::string& command);
    
    // 解包裝回應（移除 \r\n）
    static std::string unwrap(const std::string& response);
};

// Blue Bridge 輔助類別
class Helper {
public:
    explicit Helper(BLE::BLEManager& bleManager);
    ~Helper();

    // 禁止複製
    Helper(const Helper&) = delete;
    Helper& operator=(const Helper&) = delete;

    // 掃描並連線到 Blue Bridge
    bool scanAndConnect(int scanTimeMs = 5000);

    // 連線到指定地址的 Blue Bridge
    bool connect(const std::string& deviceAddress);

    // 斷線
    bool disconnect();

    // 是否已連線
    bool isConnected() const;

    // 發送命令（自動包裝格式）
    bool sendCommand(const std::string& command);

    // 接收回應（需要先訂閱通知）
    bool subscribeResponse(std::function<void(const std::string&)> callback);

    // 取消訂閱回應
    bool unsubscribeResponse();

    // 取得最後的錯誤訊息
    std::string getLastError() const;

    // 取得裝置資訊（需要先連線）
    struct DeviceInfo {
        std::string name;
        std::string address;
        bool hasServices;
    };
    DeviceInfo getDeviceInfo() const;

private:
    BLE::BLEManager& ble_;
    std::string deviceAddress_;
    std::string lastError_;
    bool servicesDiscovered_;

    void setError(const std::string& error);
};

} // namespace BlueBridge