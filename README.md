# BLE Tool - Windows BLE 通訊函式庫

Windows 平台的藍牙低功耗 (BLE) 通訊函式庫，提供 C++ 和 C API 介面。

**版本**: 1.1.0  
**最後更新**: 2025-12-30

---

## 核心特性

- BLE 裝置掃描（支援 Active/Passive 模式）
- 裝置連線與配對管理
- GATT 服務與特性發現
- 資料讀寫（支援 Write/WriteWithoutResponse）
- 通知訂閱與回調
- 雙回調掃描機制（found/updated）
- 智能裝置名稱顯示（LocalName/廠商名稱/服務名稱）
- C++ 和 C API 雙介面
- 跨語言整合支援（C#、Python、Node.js）

---

## 系統架構

```
應用層 (C++/C#/Python App)
         ↓
   ┌─────────────────┐
   │   C API Layer   │  ble_api.dll
   │  (DLL 導出)     │
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │   Core Layer    │  ble_core.lib
   │  (業務邏輯)     │
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │ Platform Layer  │  ble_platform.lib
   │ (Windows WinRT) │
   └─────────────────┘
```

### 分層說明

| 層級 | 輸出檔案 | 職責 |
|------|----------|------|
| **Platform Layer** | `ble_platform.lib` | WinRT API 封裝，處理 Advertisement/Scan Response 配對 |
| **Core Layer** | `ble_core.lib` | BLE 業務邏輯，連線管理，回調處理 |
| **C API Layer** | `ble_api.dll` | C 介面導出，供其他語言使用 |

---

## 編譯環境

- **作業系統**: Windows 10 (1809+) / Windows 11
- **編譯器**: Visual Studio 2022
- **CMake**: 3.20+
- **Windows SDK**: 10.0.19041.0+

### 編譯步驟

```batch
# 方法 1: 使用編譯腳本（推薦）
build.bat

# 方法 2: 手動編譯
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

### 編譯輸出

```
build/bin/Release/
├── ble_tool.exe      # 測試程式
└── ble_api.dll       # C API 動態庫

build/lib/Release/
├── ble_core.lib      # C++ 核心靜態庫
├── ble_platform.lib  # 平台層靜態庫
└── ble_api.lib       # DLL 匯入庫
```

---

## API 使用指南

### C++ API

#### 基本流程

```cpp
#include "BLEManager.h"
using namespace BLE;

BLEManager ble;

// 1. 初始化
ble.initialize();

// 2. 掃描裝置（雙回調模式）
auto foundCallback = [](const DeviceInfo& dev) {
    // 新裝置發現時觸發（每個裝置一次）
    std::cout << dev.name << " - " << dev.rssi << " dBm" << std::endl;
};

auto updatedCallback = [](const DeviceInfo& dev) {
    // RSSI 更新時觸發（重複廣播時）
    std::cout << "Updated: " << dev.address << std::endl;
};

ble.startScan(5000, ScanMode::Active, foundCallback, updatedCallback);

// 3. 連線
ble.connect("AA:BB:CC:DD:EE:FF");
ble.discoverServices();

// 4. 寫入資料
ble.writeString(serviceUuid, charUuid, "Hello BLE", true);

// 5. 訂閱通知
ble.subscribeNotify(serviceUuid, charUuid, 
    [](const uint8_t* data, size_t len) {
        // 處理接收的資料
    });

// 6. 清理
ble.disconnect();
ble.shutdown();
```

#### 主要資料結構

```cpp
// 裝置資訊
struct DeviceInfo {
    std::string name;                      // 裝置名稱（LocalName/廠商名稱/服務名稱）
    std::string address;                   // MAC 地址 (AA:BB:CC:DD:EE:FF)
    int16_t rssi;                          // 信號強度 (dBm)
    bool hasName;                          // 是否有 LocalName
    uint16_t manufacturerId;               // 廠商 ID
    std::vector<std::string> serviceUuids; // 服務 UUID
    uint64_t timestamp;                    // Unix timestamp (ms)
};

// 掃描模式
enum class ScanMode {
    Active = 0,   // 發送 Scan Request（可獲取 LocalName）
    Passive = 1   // 不發送（省電但可能無名稱）
};
```

#### 主要 API

```cpp
// 初始化
bool initialize();
void shutdown();

// 掃描（新版雙回調 API）
bool startScan(
    int timeoutMs,
    ScanMode mode,
    ScanFoundCallback foundCallback,      // 新裝置發現
    ScanUpdatedCallback updatedCallback   // RSSI 更新（可選，傳 nullptr）
);
void stopScan();

// 連線
bool connect(const std::string& deviceAddress);
bool connectByName(const std::string& deviceName);
bool disconnect();
bool isConnected() const;

// 服務發現
bool discoverServices();
std::vector<std::string> getServices();
std::vector<std::string> getCharacteristics(const std::string& serviceUuid);

// 資料操作
bool writeData(const std::string& serviceUuid, const std::string& charUuid,
               const uint8_t* data, size_t length, bool withResponse);
bool writeString(const std::string& serviceUuid, const std::string& charUuid,
                 const std::string& data, bool withResponse);
bool readData(const std::string& serviceUuid, const std::string& charUuid,
              std::vector<uint8_t>& outData);

// 通知訂閱
bool subscribeNotify(const std::string& serviceUuid, const std::string& charUuid,
                     DataCallback callback);
bool unsubscribeNotify(const std::string& serviceUuid, const std::string& charUuid);

// 錯誤處理
std::string getLastError() const;
```

---

### C API (DLL)

#### 基本流程

```c
#include "ble_api.h"

// 掃描回調
void onDeviceFound(const char* name, const char* address, short rssi,
                   int hasName, unsigned short mfgId, const char* services,
                   unsigned long long timestamp, void* userData) {
    printf("Found: %s (%s)\n", name, address);
}

int main() {
    // 建立實例
    BLE_HANDLE ble = BLE_Create();
    BLE_Initialize(ble);
    
    // 掃描
    BLE_StartScan(ble, 5000, BLE_SCAN_MODE_ACTIVE, 
                  onDeviceFound, NULL, NULL);
    
    // 連線
    BLE_Connect(ble, "AA:BB:CC:DD:EE:FF");
    BLE_DiscoverServices(ble);
    
    // 寫入
    const char* msg = "Hello";
    BLE_WriteData(ble, serviceUuid, charUuid, 
                  (const unsigned char*)msg, strlen(msg), 1);
    
    // 清理
    BLE_Shutdown(ble);
    BLE_Destroy(ble);
    return 0;
}
```

#### 主要函數

```c
// 初始化
BLE_HANDLE BLE_Create(void);
void BLE_Destroy(BLE_HANDLE handle);
int BLE_Initialize(BLE_HANDLE handle);
void BLE_Shutdown(BLE_HANDLE handle);

// 掃描（新版雙回調 API）
int BLE_StartScan(
    BLE_HANDLE handle,
    int timeoutMs,
    BLE_ScanMode mode,
    BLE_ScanFoundCallback foundCallback,
    BLE_ScanUpdatedCallback updatedCallback,  // 可選，傳 NULL
    void* userData
);
void BLE_StopScan(BLE_HANDLE handle);

// 連線
int BLE_Connect(BLE_HANDLE handle, const char* address);
int BLE_Disconnect(BLE_HANDLE handle);

// 服務發現
int BLE_DiscoverServices(BLE_HANDLE handle);

// 資料操作
int BLE_WriteData(BLE_HANDLE handle, const char* serviceUuid,
                  const char* charUuid, const unsigned char* data,
                  size_t length, int withResponse);
int BLE_ReadData(BLE_HANDLE handle, const char* serviceUuid,
                 const char* charUuid, unsigned char* buffer, size_t* length);

// 通知訂閱
int BLE_SubscribeNotify(BLE_HANDLE handle, const char* serviceUuid,
                        const char* charUuid, BLE_DataCallback callback,
                        void* userData);
int BLE_UnsubscribeNotify(BLE_HANDLE handle, const char* serviceUuid,
                           const char* charUuid);

// 錯誤處理
const char* BLE_GetLastError(BLE_HANDLE handle);
```

#### 回調型別定義

```c
// 掃描模式
typedef enum {
    BLE_SCAN_MODE_ACTIVE = 0,
    BLE_SCAN_MODE_PASSIVE = 1
} BLE_ScanMode;

// 裝置發現回調
typedef void (*BLE_ScanFoundCallback)(
    const char* name,
    const char* address,
    short rssi,
    int hasName,
    unsigned short manufacturerId,
    const char* serviceUuids,
    unsigned long long timestamp,
    void* userData
);

// 裝置更新回調
typedef void (*BLE_ScanUpdatedCallback)(
    const char* name,
    const char* address,
    short rssi,
    int hasName,
    unsigned short manufacturerId,
    const char* serviceUuids,
    unsigned long long timestamp,
    void* userData
);

// 資料接收回調
typedef void (*BLE_DataCallback)(
    const unsigned char* data,
    size_t length,
    void* userData
);

// 連線狀態回調
typedef void (*BLE_ConnectionCallback)(
    int connected,
    const char* message,
    void* userData
);
```

---

## 跨語言整合

### C# (P/Invoke)

```csharp
using System;
using System.Runtime.InteropServices;

public static class BleApi
{
    private const string DllName = "ble_api.dll";
    
    public enum ScanMode { Active = 0, Passive = 1 }
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void ScanFoundCallback(
        [MarshalAs(UnmanagedType.LPStr)] string name,
        [MarshalAs(UnmanagedType.LPStr)] string address,
        short rssi, int hasName, ushort mfgId,
        [MarshalAs(UnmanagedType.LPStr)] string services,
        ulong timestamp, IntPtr userData);
    
    [DllImport(DllName)]
    public static extern IntPtr BLE_Create();
    
    [DllImport(DllName)]
    public static extern int BLE_Initialize(IntPtr handle);
    
    [DllImport(DllName)]
    public static extern int BLE_StartScan(
        IntPtr handle, int timeoutMs, ScanMode mode,
        ScanFoundCallback foundCallback,
        ScanFoundCallback updatedCallback,
        IntPtr userData);
    
    [DllImport(DllName)]
    public static extern int BLE_Connect(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string address);
    
    // ... 其他函數
}
```

### Python (ctypes)

```python
from ctypes import *

ble_dll = CDLL("ble_api.dll")

# 定義回調
ScanFoundCallbackType = CFUNCTYPE(
    None, c_char_p, c_char_p, c_int16, c_int, 
    c_uint16, c_char_p, c_uint64, c_void_p)

# 定義函數
ble_dll.BLE_Create.restype = c_void_p
ble_dll.BLE_Initialize.argtypes = [c_void_p]
ble_dll.BLE_StartScan.argtypes = [
    c_void_p, c_int, c_int, 
    ScanFoundCallbackType, ScanFoundCallbackType, c_void_p]

# 使用
ble = ble_dll.BLE_Create()
ble_dll.BLE_Initialize(ble)
```

---

## 重要更新說明

### v1.1.0 (2025-12-30)

#### 雙回調掃描機制
- 新增 `ScanFoundCallback`：裝置首次發現時觸發（每個裝置一次）
- 新增 `ScanUpdatedCallback`：RSSI 更新時觸發（重複廣播時）
- 支援 Active/Passive 掃描模式

#### Advertisement/Scan Response 配對
- 自動配對 Advertisement 和 Scan Response
- 延遲觸發機制（等待 Scan Response 或超時 200ms）
- 正確提取 LocalName

#### 智能裝置名稱顯示
- 優先顯示 LocalName（從 Scan Response）
- 無 LocalName 時顯示廠商名稱（例如 "Apple"）
- 無廠商名稱時顯示服務名稱（例如 "Battery Service Device"）
- 都沒有時顯示 "Unknown Device"

#### 斷線改進
- 正確清理 GATT Session
- 移除通知訂閱的事件處理器
- 清空 services/characteristics 容器
- 修復斷線後裝置仍連線的問題

---

## 專案結構

```
BLE_Tool/
├── core/                    # 核心業務邏輯層
│   ├── include/
│   │   ├── BLEManager.h
│   │   ├── IBLEBackend.h
│   │   └── BLETypes.h
│   └── src/
│       └── BLEManager.cpp
│
├── platform/windows/        # Windows 平台實作層
│   ├── include/
│   │   └── WindowsBLE.h
│   └── src/
│       └── WindowsBLE.cpp
│
├── lib/                     # C API 導出層
│   ├── include/
│   │   └── ble_api.h
│   └── src/
│       └── ble_api.cpp
│
├── app/                     # 測試應用程式
│   └── main.cpp
│
└── CMakeLists.txt
```

---

## 常見問題

### 編譯問題

**找不到 Windows SDK**
- 確認已安裝 Windows SDK 10.0.19041.0 或更新版本
- 使用 Visual Studio Installer 安裝

**WinRT 編譯錯誤**
- 檢查 CMakeLists.txt 中的 `WINVER` 和 `_WIN32_WINNT` 定義
- 確認使用 `/std:c++17` 或更新標準

### 執行問題

**找不到 ble_api.dll**
- 確保 DLL 與 EXE 在同一目錄
- 或將 DLL 路徑加入系統 PATH

**配對失敗**
- 檢查裝置是否已在 Windows 設定中配對
- 若已配對，請先「遺忘裝置」再重新連線
- 輸入正確的 PIN 碼（通常是 000000）

**連線後無法斷線**
- 確認已更新到 v1.1.0 版本
- 檢查是否正確呼叫 `BLE_Disconnect()`

**掃描不到裝置**
- 確認裝置正在廣播
- 確認 Windows 藍牙已啟用
- 嘗試使用 Active 模式掃描

---

## 授權

內部使用專案，未對外發布。

---

## 聯絡資訊

如有問題請聯絡開發團隊。