**完整的專案 README.md**

# BLE Tool - Windows BLE Communication Library

Windows 平台的藍牙低功耗 (BLE) 通訊函式庫，提供 C++ 和 C API 介面。

---

## 版本資訊

- **版本**: 1.0.0
- **最後更新**: 2025-12-29
- **授權**: 內部使用

---

## 📋 目錄

- [專案概述](#專案概述)
- [核心特性](#核心特性)
- [系統架構](#系統架構)
- [編譯環境需求](#編譯環境需求)
- [編譯步驟](#編譯步驟)
- [專案結構](#專案結構)
- [C++ API 使用指南](#c-api-使用指南)
- [C API (DLL) 使用指南](#c-api-dll-使用指南)
- [跨語言整合](#跨語言整合)
- [開發指南](#開發指南)
- [問題排查](#問題排查)
- [設計說明](#設計說明)

---

## 專案概述

BLE Tool 是一個基於 Windows Runtime (WinRT) API 開發的藍牙 BLE 通訊函式庫，採用分層架構設計，支援跨語言整合。本專案原先設計為跨平台架構，但目前僅實作 Windows 平台支援。

### 核心特性

- ✅ **BLE 裝置掃描** - 掃描附近的 BLE 裝置，取得名稱、地址、RSSI、服務 UUID 等資訊
- ✅ **裝置連線管理** - 自動配對與連線（觸發 Windows 配對對話框）
- ✅ **GATT 服務發現** - 發現服務與特性
- ✅ **資料讀寫** - 支援 Read/Write/WriteWithoutResponse
- ✅ **通知訂閱** - 接收 BLE Notify 事件
- ✅ **回調機制** - 掃描、連線、資料接收、錯誤處理的回調支援
- ✅ **C++ 和 C API** - 原生 C++ 介面與 DLL 導出
- ✅ **跨語言整合** - 可用於 C#、Python、Node.js 等語言

---

## 系統架構

```
┌─────────────────────────────────────────────────┐
│           應用層 (Your Application)              │
│        C++ App / C# App / Python App            │
└────────────────────┬────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
┌────────▼────────┐    ┌────────▼─────────┐
│   C++ API       │    │   C API (DLL)    │
│  (BLEManager)   │    │   (ble_api.dll)  │
└────────┬────────┘    └────────┬─────────┘
         │                      │
         └──────────┬───────────┘
                    │
         ┌──────────▼───────────┐
         │   Core Layer         │
         │  (ble_core.lib)      │
         │   - BLEManager       │
         │   - 業務邏輯         │
         │   - 狀態管理         │
         └──────────┬───────────┘
                    │
         ┌──────────▼───────────┐
         │   Platform Layer     │
         │ (ble_platform.lib)   │
         │   - WindowsBLE       │
         │   - WinRT API 封裝   │
         └──────────┬───────────┘
                    │
         ┌──────────▼───────────┐
         │   Windows BLE Stack  │
         │   (作業系統層)       │
         └──────────────────────┘
```

### 分層說明

| 層級 | 目錄 | 輸出 | 職責 |
|------|------|------|------|
| **Core Layer** | `core/` | `ble_core.lib` | 提供通用 BLE 介面，管理連線狀態與回調 |
| **Platform Layer** | `platform/windows/` | `ble_platform.lib` | Windows WinRT API 實作 |
| **C API Layer** | `lib/` | `ble_api.dll` | 導出 C 介面供其他語言使用 |
| **Application** | `app/` | `ble_tool.exe` | 應用程式範例（非函式庫核心） |

---

## 編譯環境需求

### 必要工具

- **作業系統**: Windows 10 (1809+) / Windows 11
- **編譯器**: Visual Studio 2022（需包含「使用 C++ 的桌面開發」工作負載）
- **CMake**: 3.20 或更新版本
- **Windows SDK**: 10.0.19041.0 或更新版本

### 安裝 Visual Studio 2022

下載安裝程式後，確保勾選以下元件：
- ✅ 使用 C++ 的桌面開發
- ✅ Windows 10/11 SDK
- ✅ C++ CMake 工具

---

## 編譯步驟

### 方法 1: 使用編譯腳本（推薦）

1. 開啟 **Developer Command Prompt for VS 2022**
2. 切換到專案目錄
3. 執行編譯腳本：

```batch
build.bat
```

### 方法 2: 手動編譯

```batch
# 建立編譯目錄
mkdir build
cd build

# 生成專案檔
cmake -G "Visual Studio 17 2022" -A x64 ..

# 編譯 Release 版本
cmake --build . --config Release

# 或編譯 Debug 版本
cmake --build . --config Debug
```

### 編譯輸出

編譯成功後，產出檔案位於：

```
build/
├── bin/
│   └── Release/
│       ├── ble_tool.exe          # 測試程式
│       └── ble_api.dll           # C API 動態連結庫
└── lib/
    └── Release/
        ├── ble_api.exp           # DLL 導出表
        ├── ble_api.lib           # DLL 匯入庫（給其他程式連結用）
        ├── ble_core.lib          # C++ 核心靜態庫
        └── ble_platform.lib      # 平台層靜態庫
```

---

## 專案結構

```
BLE_Tool/
├── core/                          # 核心業務邏輯層
│   ├── include/
│   │   ├── BLEManager.h           # 主要介面
│   │   ├── IBLEBackend.h          # 平台抽象介面
│   │   └── BLETypes.h             # 型別定義
│   ├── src/
│   │   └── BLEManager.cpp
│   └── CMakeLists.txt
│
├── platform/                      # 平台特定實作層
│   └── windows/
│       ├── include/
│       │   └── WindowsBLE.h
│       ├── src/
│       │   └── WindowsBLE.cpp     # Windows WinRT 實作
│       └── CMakeLists.txt
│
├── lib/                           # C API 導出層
│   ├── include/
│   │   └── ble_api.h              # C API 標頭檔
│   ├── src/
│   │   └── ble_api.cpp            # DLL 導出實作
│   └── CMakeLists.txt
│
├── app/                           # 應用程式範例
│   ├── main.cpp
│   ├── blue_bridge_helper.h       # 裝置專用 Helper
│   ├── blue_bridge_helper.cpp
│   └── CMakeLists.txt
│
├── CMakeLists.txt                 # 主 CMake 設定
├── build.bat                      # Windows 編譯腳本
└── README.md                      # 本檔案
```

---

## C++ API 使用指南

### 命名空間

所有 API 位於 `BLE` 命名空間下：

```cpp
#include "BLEManager.h"
using namespace BLE;
```

### 基本使用流程

```cpp
#include "BLEManager.h"
#include <iostream>

using namespace BLE;

int main() {
    // 1. 建立 BLE Manager
    BLEManager ble;
    
    // 2. 初始化
    if (!ble.initialize()) {
        std::cerr << "Failed to initialize: " << ble.getLastError() << std::endl;
        return 1;
    }
    
    // 3. 設定掃描回調（可選）
    ble.setScanCallback([](const DeviceInfo& device) {
        std::cout << "Found: " << device.name 
                  << " (" << device.address << ")"
                  << " RSSI: " << device.rssi << " dBm" << std::endl;
    });
    
    // 4. 掃描裝置
    if (ble.startScan(5000)) {  // 掃描 5 秒
        auto devices = ble.getScannedDevices();
        std::cout << "Found " << devices.size() << " devices" << std::endl;
    }
    
    // 5. 連線到裝置
    if (!ble.connect("AA:BB:CC:DD:EE:FF")) {
        std::cerr << "Connection failed: " << ble.getLastError() << std::endl;
        return 1;
    }
    
    // 6. 發現服務
    if (!ble.discoverServices()) {
        std::cerr << "Service discovery failed" << std::endl;
        return 1;
    }
    
    auto services = ble.getServices();
    for (const auto& uuid : services) {
        std::cout << "Service: " << uuid << std::endl;
    }
    
    // 7. 寫入資料
    std::string data = "Hello BLE";
    ble.writeString(
        "0000abf0-0000-1000-8000-00805f9b34fb",  // Service UUID
        "0000abf1-0000-1000-8000-00805f9b34fb",  // Characteristic UUID
        data,
        true  // with response
    );
    
    // 8. 訂閱通知
    ble.subscribeNotify(
        "0000abf0-0000-1000-8000-00805f9b34fb",
        "0000abf1-0000-1000-8000-00805f9b34fb",
        [](const uint8_t* data, size_t length) {
            std::cout << "Received " << length << " bytes" << std::endl;
        }
    );
    
    // 9. 清理
    ble.disconnect();
    ble.shutdown();
    
    return 0;
}
```

### 完整 API 參考

#### 初始化與清理

```cpp
bool initialize();                // 初始化 BLE 系統
void shutdown();                  // 關閉 BLE 系統
bool isInitialized() const;       // 檢查是否已初始化
```

#### 裝置掃描

```cpp
// 開始掃描（阻塞式）
bool startScan(int timeoutMs = 5000);

// 停止掃描
void stopScan();

// 設定掃描回調（掃描期間即時通知）
void setScanCallback(ScanCallback callback);
// 其中 ScanCallback = std::function<void(const DeviceInfo&)>

// 取得已掃描的裝置列表
std::vector<DeviceInfo> getScannedDevices() const;
```

**DeviceInfo 結構：**

```cpp
struct DeviceInfo {
    std::string name;                      // 裝置名稱
    std::string address;                   // MAC 地址 (格式: AA:BB:CC:DD:EE:FF)
    int16_t rssi;                          // 信號強度 (dBm)
    uint16_t manufacturerId;               // 廠商 ID (0 表示無)
    std::vector<std::string> serviceUuids; // 廣播的服務 UUID 列表
    bool hasName;                          // 是否有廣播名稱
};
```

#### 連線管理

```cpp
// 連線到指定裝置（MAC 地址）
bool connect(const std::string& deviceAddress);

// 根據裝置名稱連線（需先掃描）
bool connectByName(const std::string& deviceName);

// 斷線
bool disconnect();

// 檢查連線狀態
bool isConnected() const;

// 設定連線狀態回調
void setConnectionCallback(ConnectionCallback callback);
// 其中 ConnectionCallback = std::function<void(bool connected, const std::string& message)>
```

#### 服務與特性

```cpp
// 發現 GATT 服務
bool discoverServices();

// 取得所有服務 UUID
std::vector<std::string> getServices();

// 取得指定服務的特性 UUID
std::vector<std::string> getCharacteristics(const std::string& serviceUuid);
```

#### 資料讀寫

```cpp
// 寫入原始資料
bool writeData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    const uint8_t* data,
    size_t length,
    bool withResponse = true  // true: Write, false: WriteWithoutResponse
);

// 寫入字串（便利函數）
bool writeString(
    const std::string& serviceUuid,
    const std::string& charUuid,
    const std::string& data,
    bool withResponse = true
);

// 讀取資料
bool readData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    std::vector<uint8_t>& outData
);
```

#### 通知訂閱

```cpp
// 訂閱特性通知
bool subscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid,
    DataCallback callback
);
// 其中 DataCallback = std::function<void(const uint8_t* data, size_t length)>

// 取消訂閱
bool unsubscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid
);
```

#### 錯誤處理

```cpp
// 取得最後的錯誤訊息
std::string getLastError() const;

// 設定錯誤回調
void setErrorCallback(ErrorCallback callback);
// 其中 ErrorCallback = std::function<void(const std::string& error)>
```

---

## C API (DLL) 使用指南

### API 標頭檔

```c
#include "ble_api.h"
```

### 基本使用範例 (C)

```c
#include "ble_api.h"
#include <stdio.h>

// 掃描回調
void onDeviceFound(const char* name, const char* address, int16_t rssi, void* userData) {
    printf("Found: %s (%s) RSSI: %d dBm\n", name, address, rssi);
}

// 資料接收回調
void onDataReceived(const uint8_t* data, size_t length, void* userData) {
    printf("Received %zu bytes\n", length);
}

int main() {
    // 建立 BLE 實例
    BLE_HANDLE ble = BLE_Create();
    if (!ble) {
        printf("Failed to create BLE handle\n");
        return 1;
    }
    
    // 初始化
    if (BLE_Initialize(ble) != BLE_SUCCESS) {
        printf("Initialize failed: %s\n", BLE_GetLastError(ble));
        BLE_Destroy(ble);
        return 1;
    }
    
    // 設定掃描回調
    BLE_SetScanCallback(ble, onDeviceFound, NULL);
    
    // 掃描裝置
    if (BLE_StartScan(ble, 5000) == BLE_SUCCESS) {
        printf("Scan completed\n");
    }
    
    // 連線
    if (BLE_Connect(ble, "AA:BB:CC:DD:EE:FF") == BLE_SUCCESS) {
        printf("Connected!\n");
        
        // 發現服務
        BLE_DiscoverServices(ble);
        
        // 寫入資料
        const char* message = "Hello BLE";
        BLE_WriteData(ble,
            "0000abf0-0000-1000-8000-00805f9b34fb",
            "0000abf1-0000-1000-8000-00805f9b34fb",
            (const uint8_t*)message,
            strlen(message),
            1  // with response
        );
        
        // 訂閱通知
        BLE_SubscribeNotify(ble,
            "0000abf0-0000-1000-8000-00805f9b34fb",
            "0000abf1-0000-1000-8000-00805f9b34fb",
            onDataReceived,
            NULL
        );
        
        // 斷線
        BLE_Disconnect(ble);
    }
    
    // 清理
    BLE_Shutdown(ble);
    BLE_Destroy(ble);
    
    return 0;
}
```

### 完整 C API 函數列表

#### 初始化與清理

```c
BLE_HANDLE BLE_Create(void);
void BLE_Destroy(BLE_HANDLE handle);
int BLE_Initialize(BLE_HANDLE handle);
void BLE_Shutdown(BLE_HANDLE handle);
int BLE_IsInitialized(BLE_HANDLE handle);
```

#### 掃描

```c
int BLE_StartScan(BLE_HANDLE handle, int timeoutMs);
void BLE_StopScan(BLE_HANDLE handle);
void BLE_SetScanCallback(BLE_HANDLE handle, BLE_ScanCallback callback, void* userData);
```

**回調型別：**
```c
typedef void (*BLE_ScanCallback)(
    const char* name,
    const char* address,
    int16_t rssi,
    void* userData
);
```

#### 連線

```c
int BLE_Connect(BLE_HANDLE handle, const char* deviceAddress);
int BLE_Disconnect(BLE_HANDLE handle);
int BLE_IsConnected(BLE_HANDLE handle);
void BLE_SetConnectionCallback(BLE_HANDLE handle, BLE_ConnectionCallback callback, void* userData);
```

**回調型別：**
```c
typedef void (*BLE_ConnectionCallback)(
    int connected,
    const char* message,
    void* userData
);
```

#### 服務發現

```c
int BLE_DiscoverServices(BLE_HANDLE handle);
```

#### 資料操作

```c
int BLE_WriteData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    const uint8_t* data,
    size_t length,
    int withResponse
);

int BLE_ReadData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    uint8_t* buffer,
    size_t* length
);
```

#### 通知訂閱

```c
int BLE_SubscribeNotify(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    BLE_DataCallback callback,
    void* userData
);

int BLE_UnsubscribeNotify(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid
);
```

**回調型別：**
```c
typedef void (*BLE_DataCallback)(
    const uint8_t* data,
    size_t length,
    void* userData
);
```

#### 錯誤處理

```c
const char* BLE_GetLastError(BLE_HANDLE handle);
void BLE_SetErrorCallback(BLE_HANDLE handle, BLE_ErrorCallback callback, void* userData);
```

**回調型別：**
```c
typedef void (*BLE_ErrorCallback)(
    const char* error,
    void* userData
);
```

#### 版本資訊

```c
const char* BLE_GetVersion(void);
```

---

## 跨語言整合

### C# 整合

**P/Invoke 宣告：**

```csharp
using System;
using System.Runtime.InteropServices;
using System.Text;

public class BLEApi
{
    private const string DllName = "ble_api.dll";
    
    // 回調委派
    public delegate void ScanCallback(
        [MarshalAs(UnmanagedType.LPStr)] string name,
        [MarshalAs(UnmanagedType.LPStr)] string address,
        short rssi,
        IntPtr userData);
    
    public delegate void DataCallback(
        IntPtr data,
        IntPtr length,
        IntPtr userData);
    
    public delegate void ConnectionCallback(
        int connected,
        [MarshalAs(UnmanagedType.LPStr)] string message,
        IntPtr userData);
    
    // API 函數
    [DllImport(DllName)]
    public static extern IntPtr BLE_Create();
    
    [DllImport(DllName)]
    public static extern void BLE_Destroy(IntPtr handle);
    
    [DllImport(DllName)]
    public static extern int BLE_Initialize(IntPtr handle);
    
    [DllImport(DllName)]
    public static extern int BLE_Connect(IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string address);
    
    [DllImport(DllName)]
    public static extern int BLE_WriteData(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string serviceUuid,
        [MarshalAs(UnmanagedType.LPStr)] string charUuid,
        byte[] data,
        IntPtr length,
        int withResponse);
    
    [DllImport(DllName)]
    public static extern int BLE_SubscribeNotify(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string serviceUuid,
        [MarshalAs(UnmanagedType.LPStr)] string charUuid,
        DataCallback callback,
        IntPtr userData);
    
    [DllImport(DllName, CharSet = CharSet.Ansi)]
    public static extern IntPtr BLE_GetLastError(IntPtr handle);
}
```

**使用範例：**

```csharp
class Program
{
    static void Main()
    {
        // 建立 BLE 實例
        IntPtr ble = BLEApi.BLE_Create();
        
        // 初始化
        if (BLEApi.BLE_Initialize(ble) == 0)  // BLE_SUCCESS = 0
        {
            Console.WriteLine("Initialized!");
            
            // 連線
            if (BLEApi.BLE_Connect(ble, "AA:BB:CC:DD:EE:FF") == 0)
            {
                Console.WriteLine("Connected!");
                
                // 寫入資料
                byte[] data = Encoding.UTF8.GetBytes("Hello BLE");
                BLEApi.BLE_WriteData(
                    ble,
                    "0000abf0-0000-1000-8000-00805f9b34fb",
                    "0000abf1-0000-1000-8000-00805f9b34fb",
                    data,
                    (IntPtr)data.Length,
                    1
                );
                
                // 訂閱通知
                BLEApi.BLE_SubscribeNotify(
                    ble,
                    "0000abf0-0000-1000-8000-00805f9b34fb",
                    "0000abf1-0000-1000-8000-00805f9b34fb",
                    (dataPtr, length, userData) => {
                        Console.WriteLine($"Received {length} bytes");
                    },
                    IntPtr.Zero
                );
            }
        }
        
        // 清理
        BLEApi.BLE_Destroy(ble);
    }
}
```

### Python 整合 (ctypes)

```python
import ctypes
from ctypes import *
import sys

# 載入 DLL
ble_dll = ctypes.CDLL("ble_api.dll")

# 定義回調型別
ScanCallbackType = CFUNCTYPE(None, c_char_p, c_char_p, c_int16, c_void_p)
DataCallbackType = CFUNCTYPE(None, POINTER(c_uint8), c_size_t, c_void_p)

# 定義函數原型
ble_dll.BLE_Create.restype = c_void_p
ble_dll.BLE_Initialize.argtypes = [c_void_p]
ble_dll.BLE_Initialize.restype = c_int
ble_dll.BLE_Connect.argtypes = [c_void_p, c_char_p]
ble_dll.BLE_Connect.restype = c_int
ble_dll.BLE_WriteData.argtypes = [c_void_p, c_char_p, c_char_p, POINTER(c_uint8), c_size_t, c_int]
ble_dll.BLE_WriteData.restype = c_int

# 使用範例
ble = ble_dll.BLE_Create()
if ble_dll.BLE_Initialize(ble) == 0:
    print("Initialized!")
    
    if ble_dll.BLE_Connect(ble, b"AA:BB:CC:DD:EE:FF") == 0:
        print("Connected!")
        
        # 寫入資料
        data = b"Hello BLE"
        ble_dll.BLE_WriteData(
            ble,
            b"0000abf0-0000-1000-8000-00805f9b34fb",
            b"0000abf1-0000-1000-8000-00805f9b34fb",
            data,
            len(data),
            1
        )

ble_dll.BLE_Destroy(ble)
```

### Node.js 整合 (ffi-napi)

```javascript
const ffi = require('ffi-napi');
const ref = require('ref-napi');

// 定義型別
const voidPtr = ref.refType(ref.types.void);
const ScanCallback = ffi.Function('void', ['string', 'string', 'int16', voidPtr]);
const DataCallback = ffi.Function('void', ['pointer', 'size_t', voidPtr]);

// 載入 DLL
const ble = ffi.Library('ble_api.dll', {
  'BLE_Create': [voidPtr, []],
  'BLE_Destroy': ['void', [voidPtr]],
  'BLE_Initialize': ['int', [voidPtr]],
  'BLE_Connect': ['int', [voidPtr, 'string']],
  'BLE_WriteData': ['int', [voidPtr, 'string', 'string', 'pointer', 'size_t', 'int']],
  'BLE_SubscribeNotify': ['int', [voidPtr, 'string', 'string', DataCallback, voidPtr]]
});

// 使用範例
const handle = ble.BLE_Create();
if (ble.BLE_Initialize(handle) === 0) {
  console.log('Initialized!');
  
  if (ble.BLE_Connect(handle, 'AA:BB:CC:DD:EE:FF') === 0) {
    console.log('Connected!');
    
    // 寫入資料
    const data = Buffer.from('Hello BLE');
    ble.BLE_WriteData(
      handle,
      '0000abf0-0000-1000-8000-00805f9b34fb',
      '0000abf1-0000-1000-8000-00805f9b34fb',
      data,
      data.length,
      1
    );
  }
}

ble.BLE_Destroy(handle);
```

---

## 開發指南

### 新增平台支援

雖然目前僅支援 Windows，但架構已預留跨平台擴展能力。

**新增 Linux 平台範例：**

1. 建立 `platform/linux/` 目錄
2. 實作 `IBLEBackend` 介面：

```cpp
// platform/linux/include/LinuxBLE.h
#pragma once
#include "IBLEBackend.h"

namespace BLE {

class LinuxBLE : public IBLEBackend {
public:
    bool initialize() override;
    void shutdown() override;
    bool connect(const std::string& address) override;
    // ... 實作其他介面
};

// 工廠函數
std::unique_ptr<IBLEBackend> createPlatformBackend();

} // namespace BLE
```

3. 修改主 `CMakeLists.txt`：

```cmake
if(WIN32)
    set(PLATFORM_NAME "windows")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

add_subdirectory(platform/${PLATFORM_NAME})
```

### 擴展功能

**建立裝置專用 Helper 類別：**

不修改核心函式庫，而是建立專用的 Helper 封裝：

```cpp
// MyDeviceHelper.h
#pragma once
#include "BLEManager.h"
#include <functional>

class MyDeviceHelper {
private:
    BLE::BLEManager& ble_;
    static constexpr const char* SERVICE_UUID = "0000abf0-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_UUID = "0000abf1-0000-1000-8000-00805f9b34fb";
    
public:
    MyDeviceHelper(BLE::BLEManager& ble) : ble_(ble) {}
    
    bool connect(const std::string& address) {
        return ble_.connect(address) && ble_.discoverServices();
    }
    
    bool sendCommand(const std::string& cmd) {
        std::string formatted = "\r\n" + cmd + "\r\n";  // 自定義格式
        return ble_.writeString(SERVICE_UUID, CHAR_UUID, formatted, true);
    }
    
    bool subscribeResponse(std::function<void(const std::string&)> callback) {
        return ble_.subscribeNotify(SERVICE_UUID, CHAR_UUID,
            [callback](const uint8_t* data, size_t len) {
                std::string response(reinterpret_cast<const char*>(data), len);
                // 解析處理...
                callback(response);
            }
        );
    }
};
```

### 除錯技巧

**檢查錯誤訊息：**

```cpp
if (!ble.connect(address)) {
    std::cerr << "Error: " << ble.getLastError() << std::endl;
}
```

**啟用錯誤回調：**

```cpp
ble.setErrorCallback([](const std::string& error) {
    std::cerr << "[BLE Error] " << error << std::endl;
});
```

**常見問題排查：**

| 問題 | 可能原因 | 解決方法 |
|------|----------|----------|
| 配對失敗 | 裝置已在 Windows 設定中配對 | 遺忘裝置後重新連線 |
| 連線超時 | 裝置未開機或超出範圍 | 確認裝置狀態 |
| 寫入失敗 | 特性不支援 Write | 檢查特性屬性，嘗試 WriteWithoutResponse |
| 通知無反應 | 未啟用 CCCD | 確認已呼叫 `subscribeNotify` |

---

## 問題排查

### 編譯錯誤

**CMake 找不到 Visual Studio：**
```batch
# 確認已安裝 VS 2022 並包含 C++ 工具
# 使用 VS 2022 的 Developer Command Prompt
```

**WinRT 相關編譯錯誤：**
```batch
# 確認 Windows SDK 版本 >= 10.0.19041.0
# 檢查 CMakeLists.txt 中的 WINVER 定義
```

### 執行錯誤

**找不到 ble_api.dll：**
```batch
# 確保 DLL 與 EXE 在同一目錄
# 或將 DLL 所在目錄加入 PATH
```

**配對失敗：**
- 檢查裝置是否已在 Windows 藍牙設定中配對
- 若已配對，請先「遺忘裝置」再重新連線
- 確認藍牙適配器正常運作

**連線超時：**
- 確認裝置已開機且在範圍內
- 檢查藍牙地址格式：`AA:BB:CC:DD:EE:FF`（大寫，冒號分隔）

### 檢查 DLL 導出
```batch
# 使用 dumpbin 檢查 DLL 匯出的函數
dumpbin /EXPORTS build\bin\Release\ble_api.dll
```

應該看到：
```
BLE_Create
BLE_Initialize
BLE_Connect
...
```

---

## 設計說明

### 為何採用分層架構？

1. **Core Layer (ble_core.lib)**
   - 提供通用的 BLE 操作介面
   - 不依賴特定平台
   - 可被 C++ 專案直接使用

2. **Platform Layer (ble_platform.lib)**
   - 實作 `IBLEBackend` 介面
   - Windows 版本使用 WinRT API
   - 未來可新增 Linux/macOS 支援

3. **C API Layer (ble_api.dll)**
   - 提供 C 介面供其他語言調用
   - 使用 `extern "C"` 避免 name mangling
   - 透過 Handle 隱藏 C++ 實作細節

### 為何 DLL 在 bin/ 而非 lib/？

Windows 上的標準做法：
- **可執行檔 (.exe) 和動態庫 (.dll)** → `bin/`（執行時需要）
- **靜態庫 (.lib) 和匯入庫 (.lib)** → `lib/`（編譯時連結）

這樣 `ble_tool.exe` 執行時可以直接找到同目錄的 `ble_api.dll`。

---
