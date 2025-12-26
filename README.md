# BLE Tool

藍芽低功耗 (BLE) 測試工具

## 編譯環境需求

- Windows 10/11
- Visual Studio 2022 (需包含 C++ 桌面開發工具)
- CMake 3.20+
- Windows SDK 10.0.19041.0+

## 編譯步驟

1. 開啟 Developer Command Prompt for VS 2022
2. 執行編譯腳本：
```
   build.bat
```

## 輸出檔案

- `build/bin/Release/ble_tool.exe` - 測試程式
- `build/lib/Release/ble_api.dll` - C API DLL（供 C# 使用）

## 專案結構
```
BLE_Tool/
├── core/           # 核心業務邏輯層
├── platform/       # 平台特定實作層
├── lib/            # C API 導出層 (DLL)
└── app/            # 測試應用程式
```

## 目前功能

- ✅ BLE 裝置掃描
- ⏳ 裝置連線（開發中）
- ⏳ 資料讀寫（開發中）
- ⏳ 通知訂閱（開發中）