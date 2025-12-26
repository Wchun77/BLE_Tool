#include "BLEManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace BLE;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    BLE Test Tool - Version 1.1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    BLEManager ble;

    // 初始化
    std::cout << "[1/3] Initializing BLE..." << std::endl;
    if (!ble.initialize()) {
        std::cerr << "ERROR: Failed to initialize BLE" << std::endl;
        std::cerr << "Reason: " << ble.getLastError() << std::endl;
        std::cout << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    std::cout << "OK: BLE initialized successfully" << std::endl;
    std::cout << std::endl;

    // 設定掃描回調
    int deviceCount = 0;
    int namedDevices = 0;
    
    ble.setScanCallback([&deviceCount, &namedDevices](const DeviceInfo& dev) {
        deviceCount++;
        if (dev.hasName && dev.name != "Unknown Device") {
            namedDevices++;
        }
        
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "[Device #" << deviceCount << "]" << std::endl;
        std::cout << "  Name         : " << dev.name;
        if (dev.hasName) {
            std::cout << " (Advertised)";
        }
        std::cout << std::endl;
        
        std::cout << "  Address      : " << dev.address << std::endl;
        std::cout << "  RSSI         : " << dev.rssi << " dBm" << std::endl;
        
        // 顯示廠商資訊
        if (dev.manufacturerId != 0) {
            std::cout << "  Manufacturer : 0x" << std::hex << std::setw(4) 
                      << std::setfill('0') << dev.manufacturerId << std::dec << std::endl;
        }
        
        // 顯示服務 UUID
        if (!dev.serviceUuids.empty()) {
            std::cout << "  Services     : " << std::endl;
            for (size_t i = 0; i < dev.serviceUuids.size() && i < 3; i++) {
                std::cout << "    - " << dev.serviceUuids[i] << std::endl;
            }
            if (dev.serviceUuids.size() > 3) {
                std::cout << "    ... and " << (dev.serviceUuids.size() - 3) 
                          << " more" << std::endl;
            }
        }
        std::cout << std::endl;
    });

    // 開始掃描
    std::cout << "[2/3] Starting BLE scan (10 seconds)..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (!ble.startScan(10000)) {
        std::cerr << "ERROR: Failed to start scan" << std::endl;
        std::cerr << "Reason: " << ble.getLastError() << std::endl;
        ble.shutdown();
        std::cout << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    // 等待掃描完成
    std::this_thread::sleep_for(std::chrono::seconds(11));
    
    // 停止掃描
    std::cout << "========================================" << std::endl;
    ble.stopScan();

    // 顯示結果
    std::cout << std::endl;
    std::cout << "[3/3] Scan complete!" << std::endl;
    std::cout << "Total devices found    : " << deviceCount << std::endl;
    std::cout << "Devices with name      : " << namedDevices << std::endl;
    std::cout << "Devices without name   : " << (deviceCount - namedDevices) << std::endl;
    std::cout << std::endl;

    // 清理
    ble.shutdown();

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}