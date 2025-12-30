#include "BLEManager.h"
#include <iostream>
#include <iomanip>

using namespace BLE;

void printDeviceInfo(const DeviceInfo& dev) {
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Address:  " << dev.address << std::endl;
    std::cout << "Name:     " << dev.name << std::endl;  // 直接顯示
    std::cout << "RSSI:     " << dev.rssi << " dBm" << std::endl;
    std::cout << "Has Name: " << (dev.hasName ? "YES" : "NO") << std::endl;
    
    if (dev.manufacturerId != 0) {
        std::cout << "Mfg ID:   0x" << std::hex << std::setfill('0') 
                  << std::setw(4) << dev.manufacturerId << std::dec << std::endl;
    }
    
    if (!dev.serviceUuids.empty()) {
        std::cout << "Services: ";
        for (size_t i = 0; i < dev.serviceUuids.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << dev.serviceUuids[i];
        }
        std::cout << std::endl;
    }
    
    std::cout << "Timestamp: " << dev.timestamp << " ms" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

int main() {
    BLEManager ble;
    
    if (!ble.initialize()) {
        std::cerr << "Failed to initialize: " << ble.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << "BLE initialized successfully!" << std::endl;
    std::cout << "Starting scan for 10 seconds..." << std::endl;
    std::cout << std::endl;
    
    int foundCount = 0;
    int updateCount = 0;
    
    auto foundCallback = [&foundCount](const DeviceInfo& dev) {
        foundCount++;
        std::cout << "[FOUND #" << foundCount << "]" << std::endl;
        printDeviceInfo(dev);
    };
    
    auto updatedCallback = [&updateCount](const DeviceInfo& dev) {
        updateCount++;
        // std::cout << "[UPDATED #" << updateCount << "] " 
        //           << dev.address << " - RSSI: " << dev.rssi << " dBm" << std::endl;
    };
    
    if (!ble.startScan(10000, ScanMode::Active, foundCallback, updatedCallback)) {
        std::cerr << "Failed to start scan: " << ble.getLastError() << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Scan completed!" << std::endl;
    std::cout << "Found devices: " << foundCount << std::endl;
    std::cout << "Updates received: " << updateCount << std::endl;
    std::cout << "========================================" << std::endl;
    
    ble.shutdown();
    return 0;
}