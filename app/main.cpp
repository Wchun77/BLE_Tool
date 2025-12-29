#include "BLEManager.h"
#include "blue_bridge_helper.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace BLE;

// ANSI 顏色碼
namespace Color {
    const std::string RESET = "\033[0m";
    const std::string GREEN = "\033[32m";
    const std::string CYAN = "\033[36m";
    const std::string YELLOW = "\033[33m";
    const std::string RED = "\033[31m";
    const std::string GRAY = "\033[90m";
}

void enableAnsiColors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void printHeader() {
    std::cout << Color::CYAN;
    std::cout << "========================================" << std::endl;
    std::cout << "  Blue Bridge Test Tool - v3.1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << Color::RESET << std::endl;
}

void printHelp() {
    std::cout << std::endl;
    std::cout << Color::YELLOW << "Available commands:" << Color::RESET << std::endl;
    std::cout << "  " << Color::GREEN << "help" << Color::RESET << "       - Show this help" << std::endl;
    std::cout << "  " << Color::GREEN << "exit" << Color::RESET << "       - Exit program" << std::endl;
    std::cout << "  " << Color::GREEN << "clear" << Color::RESET << "      - Clear screen" << std::endl;
    std::cout << "  " << Color::GREEN << "disconnect" << Color::RESET << " - Disconnect from device" << std::endl;
    std::cout << std::endl;
}

void printError(const std::string& error) {
    std::cout << Color::RED << "ERROR: " << error << Color::RESET << std::endl;
}

void printWarning(const std::string& warning) {
    std::cout << Color::YELLOW << "WARNING: " << warning << Color::RESET << std::endl;
}

void printInfo(const std::string& info) {
    std::cout << Color::GRAY << info << Color::RESET << std::endl;
}

int main() {
    enableAnsiColors();
    printHeader();

    // 初始化
    std::cout << "[1/4] Initializing BLE..." << std::endl;
    BLEManager ble;
    
    if (!ble.initialize()) {
        printError("Failed to initialize BLE");
        printError("Reason: " + ble.getLastError());
        std::cout << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    std::cout << Color::GREEN << "BLE initialized" << Color::RESET << std::endl;
    std::cout << std::endl;

    // 建立 Helper
    std::cout << "[2/4] Creating Blue Bridge helper..." << std::endl;
    BlueBridge::Helper helper(ble);
    std::cout << Color::GREEN << "Helper created" << Color::RESET << std::endl;
    std::cout << std::endl;

    // 連線
    std::cout << "[3/4] Connecting to Blue Bridge..." << std::endl;
    printInfo("Address: 47:F1:2B:7D:DB:91");
    std::cout << std::endl;
    
    if (!helper.connect("47:F1:2B:7D:DB:91")) {
        printError(helper.getLastError());
        std::cout << std::endl;
        std::cout << "Please check:" << std::endl;
        std::cout << "1. Device is powered on and nearby" << std::endl;
        std::cout << "2. Device is paired in Windows Bluetooth settings" << std::endl;
        std::cout << "3. No other application is connected to the device" << std::endl;
        std::cout << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    std::cout << std::endl;

    // 訂閱回應
    std::cout << "[4/4] Subscribing to responses..." << std::endl;
    
    if (!helper.subscribeResponse([](const std::string& response) {
        // 清除當前輸入行
        std::cout << "\r\033[K";
        std::cout.flush();
        
        // 顯示 RX（無空行）
        std::cout << Color::CYAN << "[RX] <<< " << Color::RESET << response << std::endl;
        std::cout.flush();
        
        // 重新顯示提示（無空行）
        std::cout << Color::YELLOW << "Enter Command: " << Color::RESET;
        std::cout.flush();
    })) {
        printWarning("Failed to subscribe to responses");
        printWarning("Reason: " + helper.getLastError());
        std::cout << "Continuing anyway..." << std::endl;
    } else {
        std::cout << Color::GREEN << "Ready to receive responses" << Color::RESET << std::endl;
    }
    std::cout << std::endl;

    // 顯示裝置資訊
    auto devInfo = helper.getDeviceInfo();
    std::cout << Color::CYAN << "========================================" << std::endl;
    std::cout << "Device Information:" << std::endl;
    std::cout << "  Address : " << devInfo.address << std::endl;
    std::cout << "  Status  : Connected" << std::endl;
    std::cout << "========================================" << Color::RESET << std::endl;
    
    printHelp();

    // 互動式命令迴圈
    std::string command;
    while (true) {
        // 顯示提示（無空行）
        std::cout << Color::YELLOW << "Enter Command: " << Color::RESET;
        std::cout.flush();
        
        std::getline(std::cin, command);

        // 移除前後空白
        command.erase(0, command.find_first_not_of(" \t\r\n"));
        command.erase(command.find_last_not_of(" \t\r\n") + 1);

        if (command.empty()) {
            continue;
        }

        // 處理內建命令
        if (command == "exit") {
            std::cout << std::endl;
            printInfo("Exiting...");
            break;
        }
        
        if (command == "help") {
            printHelp();
            continue;
        }
        
        if (command == "clear" || command == "cls") {
            clearScreen();
            printHeader();
            
            std::cout << Color::CYAN << "========================================" << std::endl;
            std::cout << "Device: " << devInfo.address << " (Connected)" << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << std::endl;
            continue;
        }
        
        if (command == "disconnect") {
            std::cout << std::endl;
            printInfo("Disconnecting...");
            helper.disconnect();
            std::cout << Color::RED << "Disconnected" << Color::RESET << std::endl;
            std::cout << "Type 'exit' to quit." << std::endl;
            std::cout << std::endl;
            continue;
        }

        // 顯示 TX（無空行）
        std::cout << Color::GREEN << "[TX] >>> " << Color::RESET << command << std::endl;
        std::cout.flush();
        
        // 發送命令
        if (!helper.sendCommand(command)) {
            printError("Failed to send command");
            printError("Reason: " + helper.getLastError());
        }
    }

    // 清理
    std::cout << std::endl;
    printInfo("Cleaning up...");
    helper.disconnect();
    ble.shutdown();
    
    std::cout << Color::GREEN << "Goodbye!" << Color::RESET << std::endl;
    std::cout << std::endl;
    
    return 0;
}