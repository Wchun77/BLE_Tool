#include "blue_bridge_helper.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace BlueBridge {

// ============================================================================
// CommandWrapper 實作
// ============================================================================

std::string CommandWrapper::wrap(const std::string& command) {
    return "\r\n" + command + "\r\n";
}

std::string CommandWrapper::unwrap(const std::string& response) {
    std::string result = response;
    
    // 移除開頭的 \r\n
    while (result.size() >= 2 && result.substr(0, 2) == "\r\n") {
        result = result.substr(2);
    }
    
    // 移除結尾的 \r\n
    while (result.size() >= 2 && result.substr(result.size() - 2) == "\r\n") {
        result = result.substr(0, result.size() - 2);
    }
    
    return result;
}

// ============================================================================
// Helper 實作
// ============================================================================

Helper::Helper(BLE::BLEManager& bleManager)
    : ble_(bleManager)
    , servicesDiscovered_(false)
{
}

Helper::~Helper() {
    disconnect();
}

bool Helper::scanAndConnect(int scanTimeMs) {
    try {
        std::cout << "Scanning for Blue Bridge service..." << std::endl;
        
        bool found = false;
        std::string foundAddress;
        
        // ✅ 改成用 Service UUID 找
        ble_.setScanCallback([&found, &foundAddress](const BLE::DeviceInfo& dev) {
            // 檢查是否有目標服務
            for (const auto& uuid : dev.serviceUuids) {
                if (uuid == SERVICE_UUID || uuid.find("00000d20") != std::string::npos) {
                    found = true;
                    foundAddress = dev.address;
                    std::cout << "  Found Blue Bridge!" << std::endl;
                    std::cout << "    Address: " << dev.address << std::endl;
                    std::cout << "    RSSI: " << dev.rssi << " dBm" << std::endl;
                    return;
                }
            }
        });
        
        if (!ble_.startScan(scanTimeMs)) {
            setError("Failed to start scan: " + ble_.getLastError());
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(scanTimeMs + 1000));
        ble_.stopScan();
        
        if (!found) {
            setError("Blue Bridge service not found (UUID: " + SERVICE_UUID + ")");
            return false;
        }
        
        return connect(foundAddress);
        
    } catch (const std::exception& e) {
        setError(std::string("Exception in scanAndConnect: ") + e.what());
        return false;
    }
}

bool Helper::connect(const std::string& deviceAddress) {
    try {
        std::cout << "Connecting to " << deviceAddress << "..." << std::endl;
        
        deviceAddress_ = deviceAddress;
        
        // 設定連線回調
        ble_.setConnectionCallback([](bool connected, const std::string& message) {
            if (connected) {
                std::cout << "  Connected: " << message << std::endl;
            } else {
                std::cout << "  Disconnected: " << message << std::endl;
            }
        });
        
        // 連線
        if (!ble_.connect(deviceAddress)) {
            setError("Failed to connect: " + ble_.getLastError());
            return false;
        }
        
        // 等待連線穩定
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // 發現服務
        std::cout << "Discovering services..." << std::endl;
        if (!ble_.discoverServices()) {
            setError("Failed to discover services: " + ble_.getLastError());
            return false;
        }
        
        servicesDiscovered_ = true;
        
        // 驗證服務和特性
        auto services = ble_.getServices();
        bool serviceFound = false;
        
        for (const auto& svc : services) {
            if (svc == SERVICE_UUID) {
                serviceFound = true;
                std::cout << "  Service found: " << SERVICE_UUID << std::endl;
                
                auto chars = ble_.getCharacteristics(SERVICE_UUID);
                for (const auto& ch : chars) {
                    std::cout << "    Characteristic: " << ch << std::endl;
                }
                break;
            }
        }
        
        if (!serviceFound) {
            setError("Blue Bridge service not found");
            return false;
        }
        
        std::cout << "Ready!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception in connect: ") + e.what());
        return false;
    }
}

bool Helper::disconnect() {
    if (ble_.isConnected()) {
        unsubscribeResponse();
        return ble_.disconnect();
    }
    return true;
}

bool Helper::isConnected() const {
    return ble_.isConnected() && servicesDiscovered_;
}

bool Helper::sendCommand(const std::string& command) {
    if (!isConnected()) {
        setError("Not connected");
        return false;
    }
    
    try {
        // 包裝命令
        std::string wrappedCmd = CommandWrapper::wrap(command);
        
        // 發送
        bool success = ble_.writeString(SERVICE_UUID, CHAR_UUID, wrappedCmd, true);
        
        if (!success) {
            setError("Failed to send command: " + ble_.getLastError());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception in sendCommand: ") + e.what());
        return false;
    }
}

bool Helper::subscribeResponse(std::function<void(const std::string&)> callback) {
    if (!isConnected()) {
        setError("Not connected");
        return false;
    }
    
    try {
        bool success = ble_.subscribeNotify(
            SERVICE_UUID,
            CHAR_UUID,
            [callback](const uint8_t* data, size_t len) {
                std::string raw(reinterpret_cast<const char*>(data), len);
                std::string unwrapped = CommandWrapper::unwrap(raw);
                
                // ✅ 更徹底地移除所有前後的 \r 和 \n
                size_t start = 0;
                while (start < unwrapped.length() && 
                       (unwrapped[start] == '\r' || unwrapped[start] == '\n' || 
                        unwrapped[start] == ' ' || unwrapped[start] == '\t')) {
                    start++;
                }
                
                size_t end = unwrapped.length();
                while (end > start && 
                       (unwrapped[end-1] == '\r' || unwrapped[end-1] == '\n' || 
                        unwrapped[end-1] == ' ' || unwrapped[end-1] == '\t')) {
                    end--;
                }
                
                if (start >= end) {
                    return;  // 完全是空白，忽略
                }
                
                std::string cleaned = unwrapped.substr(start, end - start);
                
                // 每收到一行就立即回調
                if (callback) {
                    callback(cleaned);
                }
            }
        );
        
        if (!success) {
            setError("Failed to subscribe: " + ble_.getLastError());
        }
        
        return success;
        
    } catch (const std::exception& e) {
        setError(std::string("Exception: ") + e.what());
        return false;
    }
}

bool Helper::unsubscribeResponse() {
    if (!isConnected()) {
        return true;
    }
    
    return ble_.unsubscribeNotify(SERVICE_UUID, CHAR_UUID);
}

std::string Helper::getLastError() const {
    return lastError_;
}

Helper::DeviceInfo Helper::getDeviceInfo() const {
    DeviceInfo info;
    info.name = DEVICE_NAME;
    info.address = deviceAddress_;
    info.hasServices = servicesDiscovered_;
    return info;
}

void Helper::setError(const std::string& error) {
    lastError_ = error;
    std::cerr << "ERROR: " << error << std::endl;
}

} // namespace BlueBridge