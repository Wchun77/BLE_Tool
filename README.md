# BLE Tool - Windows BLE 通訊函式庫

Windows 平台的藍牙低功耗 (BLE) 通訊函式庫，提供完整的 C++ 和 C API 介面，適用於 BLE 裝置掃描、連線管理、GATT 服務操作及跨語言整合。

**版本**: 1.1.0  
**最後更新**: 2025-12-30

---

## 核心特性

### 掃描功能
- BLE 裝置掃描（支援 Active/Passive 模式）
- 雙回調掃描機制（found/updated）
- 智能裝置名稱顯示（LocalName/廠商名稱/服務名稱）
- Advertisement/Scan Response 自動配對

### 連線管理
- 裝置連線與斷線
- 連線狀態回調
- 根據地址或名稱連線
- 完整的 GATT Session 清理

### 資料操作
- GATT 服務與特性發現
- 資料讀寫（支援 Write/WriteWithoutResponse）
- 通知訂閱與回調
- 錯誤處理機制

### 跨語言支援
- C++ 原生 API
- C API (DLL) 導出
- 支援 C#、Python、Node.js 整合

---

## 系統架構

```
應用層 (C++/C#/Python/等)
         ↓
   ┌─────────────────┐
   │   C API Layer   │  ble_api.dll
   │  (DLL 導出)     │  ← C 介面封裝
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │   Core Layer    │  ble_core.lib
   │  (業務邏輯)     │  ← BLE 管理邏輯
   └────────┬────────┘
            ↓
   ┌─────────────────┐
   │ Platform Layer  │  ble_platform.lib
   │ (Windows WinRT) │  ← WinRT API 封裝
   └─────────────────┘
```

### 分層說明

| 層級 | 輸出檔案 | 職責 |
|------|----------|------|
| **Platform Layer** | `ble_platform.lib` | 封裝 Windows WinRT BLE API，處理 Advertisement/Scan Response 配對 |
| **Core Layer** | `ble_core.lib` | 實作 BLE 業務邏輯、裝置管理、連線管理、回調處理 |
| **C API Layer** | `ble_api.dll` | 提供 C 語言介面導出，供 C#/Python/等語言呼叫 |

---

## 專案結構

```
BLE_Tool/
├── core/                    # 核心業務邏輯層
│   ├── include/
│   │   ├── BLEManager.h     # BLE 管理器主介面
│   │   ├── BLEDevice.h      # 裝置資料結構
│   │   ├── IBLEBackend.h    # 平台抽象介面
│   │   └── BLETypes.h       # 共用型別定義
│   └── src/
│       ├── BLEManager.cpp
│       └── BLEDevice.cpp
│
├── platform/                # 平台實作層
│   └── windows/
│       ├── include/
│       │   └── WindowsBLE.h
│       └── src/
│           └── WindowsBLE.cpp  # WinRT BLE 實作
│
├── lib/                     # C API 導出層
│   ├── include/
│   │   └── ble_api.h        # C API 標頭檔
│   └── src/
│       └── ble_api.cpp      # C API 實作
│
├── tests/                   # 單元測試
│   ├── test_ble.cpp
│   └── test_scan.cpp
│
├── CMakeLists.txt           # 根專案 CMake 設定
├── build.bat                # Windows 編譯腳本
└── README.md
```

---

## 編譯環境

### 系統需求
- **作業系統**: Windows 10 (1809+) / Windows 11
- **編譯器**: Visual Studio 2022
- **CMake**: 3.20 或更新版本
- **Windows SDK**: 10.0.19041.0 或更新版本

### 編譯步驟

#### 方法 1: 使用編譯腳本（推薦）
```batch
build.bat
```

#### 方法 2: 手動編譯
```batch
# 建立 build 目錄並生成 Visual Studio 專案
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..

# 編譯 Release 版本
cmake --build . --config Release
```

### 編譯輸出

編譯完成後，輸出檔案位於：

```
build/
├── bin/Release/
│   ├── ble_tool.exe      # 測試執行檔
│   └── ble_api.dll       # C API 動態庫
│
└── lib/Release/
    ├── ble_core.lib      # C++ 核心靜態庫
    ├── ble_platform.lib  # 平台層靜態庫
    └── ble_api.lib       # DLL 匯入庫
```

---

## API 使用指南

### C++ API

#### 基本使用流程

```cpp
#include "BLEManager.h"
using namespace BLE;

int main() {
    BLEManager ble;
    
    // 1. 初始化
    if (!ble.initialize()) {
        std::cerr << "初始化失敗: " << ble.getLastError() << std::endl;
        return 1;
    }
    
    // 2. 掃描裝置（雙回調模式）
    auto foundCallback = [](const DeviceInfo& dev) {
        // 新裝置發現時觸發（每個裝置僅一次）
        std::cout << "發現裝置: " << dev.name 
                  << " (" << dev.address << ")"
                  << " RSSI: " << dev.rssi << " dBm" << std::endl;
    };
    
    auto updatedCallback = [](const DeviceInfo& dev) {
        // RSSI 更新時觸發（重複廣播時）
        std::cout << "更新 RSSI: " << dev.address 
                  << " -> " << dev.rssi << " dBm" << std::endl;
    };
    
    ble.startScan(5000, ScanMode::Active, foundCallback, updatedCallback);
    
    // 3. 連線到裝置
    if (!ble.connect("AA:BB:CC:DD:EE:FF")) {
        std::cerr << "連線失敗" << std::endl;
        return 1;
    }
    
    // 4. 發現 GATT 服務
    ble.discoverServices();
    
    // 5. 寫入資料到特性
    std::string serviceUuid = "0000180a-0000-1000-8000-00805f9b34fb";
    std::string charUuid = "00002a29-0000-1000-8000-00805f9b34fb";
    ble.writeString(serviceUuid, charUuid, "Hello BLE", true);
    
    // 6. 訂閱通知
    ble.subscribeNotify(serviceUuid, charUuid, 
        [](const uint8_t* data, size_t len) {
            std::cout << "收到通知: " 
                      << std::string((char*)data, len) << std::endl;
        });
    
    // 7. 斷線與清理
    ble.disconnect();
    ble.shutdown();
    
    return 0;
}
```

#### 主要資料結構

```cpp
namespace BLE {

// 掃描模式
enum class ScanMode {
    Active = 0,   // 發送 Scan Request（可獲取 LocalName）
    Passive = 1   // 不發送 Scan Request（省電但可能無名稱）
};

// 裝置資訊
struct DeviceInfo {
    std::string name;                      // 裝置名稱
    std::string address;                   // MAC 地址 (AA:BB:CC:DD:EE:FF)
    int16_t rssi;                          // 信號強度 (dBm)
    bool hasName;                          // 是否有 LocalName
    uint16_t manufacturerId;               // 廠商 ID (0 表示無)
    std::vector<std::string> serviceUuids; // 服務 UUID 列表
    uint64_t timestamp;                    // Unix timestamp (ms)
};

// 回調函數型別
using ScanFoundCallback = std::function<void(const DeviceInfo&)>;
using ScanUpdatedCallback = std::function<void(const DeviceInfo&)>;
using DataCallback = std::function<void(const uint8_t*, size_t)>;
using ConnectionCallback = std::function<void(bool connected, const std::string& message)>;
using ErrorCallback = std::function<void(const std::string& error)>;

} // namespace BLE
```

#### BLEManager 主要 API

```cpp
class BLEManager {
public:
    // === 初始化與清理 ===
    bool initialize();
    void shutdown();
    bool isInitialized() const;

    // === 掃描 ===
    bool startScan(
        int timeoutMs,                    // 掃描時長 (ms)
        ScanMode mode,                    // Active/Passive
        ScanFoundCallback foundCallback,  // 新裝置發現回調
        ScanUpdatedCallback updatedCallback // RSSI 更新回調（可選，傳 nullptr）
    );
    void stopScan();
    std::vector<DeviceInfo> getScannedDevices() const;

    // === 連線 ===
    bool connect(const std::string& deviceAddress);
    bool connectByName(const std::string& deviceName);
    bool disconnect();
    bool isConnected() const;
    void setConnectionCallback(ConnectionCallback callback);

    // === 服務與特性 ===
    bool discoverServices();
    std::vector<std::string> getServices();
    std::vector<std::string> getCharacteristics(const std::string& serviceUuid);

    // === 資料讀寫 ===
    bool writeData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        const uint8_t* data,
        size_t length,
        bool withResponse = true  // true: Write, false: WriteWithoutResponse
    );

    bool writeString(
        const std::string& serviceUuid,
        const std::string& charUuid,
        const std::string& data,
        bool withResponse = true
    );

    bool readData(
        const std::string& serviceUuid,
        const std::string& charUuid,
        std::vector<uint8_t>& outData
    );

    // === 通知訂閱 ===
    bool subscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid,
        DataCallback callback
    );

    bool unsubscribeNotify(
        const std::string& serviceUuid,
        const std::string& charUuid
    );

    // === 錯誤處理 ===
    std::string getLastError() const;
    void setErrorCallback(ErrorCallback callback);
};
```

---

### C API (DLL)

#### 基本使用流程

```c
#include "ble_api.h"
#include <stdio.h>

// 掃描回調函數
void onDeviceFound(const char* name, const char* address, short rssi,
                   int hasName, unsigned short mfgId, const char* services,
                   unsigned long long timestamp, void* userData) {
    printf("發現裝置: %s (%s) RSSI: %d dBm\n", name, address, rssi);
}

int main() {
    // 建立 BLE 實例
    BLE_HANDLE ble = BLE_Create();
    if (!ble) {
        printf("建立實例失敗\n");
        return 1;
    }
    
    // 初始化
    if (BLE_Initialize(ble) != BLE_SUCCESS) {
        printf("初始化失敗\n");
        BLE_Destroy(ble);
        return 1;
    }
    
    // 掃描裝置
    BLE_StartScan(ble, 5000, BLE_SCAN_MODE_ACTIVE, 
                  onDeviceFound, NULL, NULL);
    
    // 連線
    if (BLE_Connect(ble, "AA:BB:CC:DD:EE:FF") != BLE_SUCCESS) {
        printf("連線失敗: %s\n", BLE_GetLastError(ble));
        BLE_Shutdown(ble);
        BLE_Destroy(ble);
        return 1;
    }
    
    // 發現服務
    BLE_DiscoverServices(ble);
    
    // 寫入資料
    const char* msg = "Hello BLE";
    BLE_WriteData(ble, serviceUuid, charUuid,
                  (const unsigned char*)msg, strlen(msg), 1);
    
    // 清理
    BLE_Disconnect(ble);
    BLE_Shutdown(ble);
    BLE_Destroy(ble);
    
    return 0;
}
```

#### 主要函數

```c
// === 初始化/清理 ===
BLE_HANDLE BLE_Create(void);
void BLE_Destroy(BLE_HANDLE handle);
int BLE_Initialize(BLE_HANDLE handle);
void BLE_Shutdown(BLE_HANDLE handle);

// === 掃描 ===
int BLE_StartScan(
    BLE_HANDLE handle,
    int timeoutMs,
    BLE_ScanMode mode,
    BLE_ScanFoundCallback foundCallback,
    BLE_ScanUpdatedCallback updatedCallback,  // 可選，傳 NULL
    void* userData
);
void BLE_StopScan(BLE_HANDLE handle);

// === 連線 ===
int BLE_Connect(BLE_HANDLE handle, const char* address);
int BLE_Disconnect(BLE_HANDLE handle);
int BLE_IsConnected(BLE_HANDLE handle);
void BLE_SetConnectionCallback(
    BLE_HANDLE handle,
    BLE_ConnectionCallback callback,
    void* userData
);

// === 服務/特性 ===
int BLE_DiscoverServices(BLE_HANDLE handle);

// === 資料讀寫 ===
int BLE_WriteData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    const unsigned char* data,
    size_t length,
    int withResponse  // 1: Write, 0: WriteWithoutResponse
);

int BLE_ReadData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    unsigned char* buffer,
    size_t* length
);

// === 通知訂閱 ===
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

// === 錯誤處理 ===
const char* BLE_GetLastError(BLE_HANDLE handle);
void BLE_SetErrorCallback(
    BLE_HANDLE handle,
    BLE_ErrorCallback callback,
    void* userData
);

// === 版本資訊 ===
const char* BLE_GetVersion(void);
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

// 錯誤回調
typedef void (*BLE_ErrorCallback)(
    const char* error,
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
        short rssi,
        int hasName,
        ushort mfgId,
        [MarshalAs(UnmanagedType.LPStr)] string services,
        ulong timestamp,
        IntPtr userData);
    
    [DllImport(DllName)]
    public static extern IntPtr BLE_Create();
    
    [DllImport(DllName)]
    public static extern int BLE_Initialize(IntPtr handle);
    
    [DllImport(DllName)]
    public static extern int BLE_StartScan(
        IntPtr handle,
        int timeoutMs,
        ScanMode mode,
        ScanFoundCallback foundCallback,
        ScanFoundCallback updatedCallback,
        IntPtr userData);
    
    [DllImport(DllName)]
    public static extern int BLE_Connect(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string address);
    
    [DllImport(DllName)]
    public static extern void BLE_Shutdown(IntPtr handle);
    
    [DllImport(DllName)]
    public static extern void BLE_Destroy(IntPtr handle);
}

// 使用範例
class Program
{
    static void Main()
    {
        var ble = BleApi.BLE_Create();
        BleApi.BLE_Initialize(ble);
        
        BleApi.ScanFoundCallback callback = (name, addr, rssi, hasName, mfg, svc, ts, ud) =>
        {
            Console.WriteLine($"發現裝置: {name} ({addr}) RSSI: {rssi}");
        };
        
        BleApi.BLE_StartScan(ble, 5000, BleApi.ScanMode.Active, 
                             callback, null, IntPtr.Zero);
        
        BleApi.BLE_Shutdown(ble);
        BleApi.BLE_Destroy(ble);
    }
}
```

### Python (ctypes)

```python
from ctypes import *
import time

# 載入 DLL
ble_dll = CDLL("ble_api.dll")

# 定義回調函數型別
ScanFoundCallbackType = CFUNCTYPE(
    None,
    c_char_p,  # name
    c_char_p,  # address
    c_int16,   # rssi
    c_int,     # hasName
    c_uint16,  # manufacturerId
    c_char_p,  # serviceUuids
    c_uint64,  # timestamp
    c_void_p   # userData
)

# 定義函數原型
ble_dll.BLE_Create.restype = c_void_p
ble_dll.BLE_Initialize.argtypes = [c_void_p]
ble_dll.BLE_Initialize.restype = c_int
ble_dll.BLE_StartScan.argtypes = [
    c_void_p, c_int, c_int,
    ScanFoundCallbackType, ScanFoundCallbackType, c_void_p
]
ble_dll.BLE_StartScan.restype = c_int

# 掃描回調
def on_device_found(name, addr, rssi, hasName, mfgId, services, ts, userData):
    print(f"發現裝置: {name.decode('utf-8')} ({addr.decode('utf-8')}) RSSI: {rssi}")

# 使用範例
callback = ScanFoundCallbackType(on_device_found)

ble = ble_dll.BLE_Create()
ble_dll.BLE_Initialize(ble)
ble_dll.BLE_StartScan(ble, 5000, 0, callback, None, None)

time.sleep(5)

ble_dll.BLE_Shutdown(ble)
ble_dll.BLE_Destroy(ble)
```

---

## 重要更新說明

### v1.1.0 (2025-12-30)

#### 雙回調掃描機制
- **新增** `ScanFoundCallback`：裝置首次發現時觸發（每個裝置一次）
- **新增** `ScanUpdatedCallback`：RSSI 更新時觸發（重複廣播時）
- **支援** Active/Passive 掃描模式切換

#### Advertisement/Scan Response 配對
- **自動配對** Advertisement 和 Scan Response 封包
- **延遲觸發** 機制：等待 Scan Response 或超時 200ms
- **正確提取** LocalName 從 Scan Response 的 Complete Local Name

#### 智能裝置名稱顯示
- **優先顯示** LocalName（從 Scan Response）
- **次要顯示** 廠商名稱（例如 "Apple", "Nordic"）
- **備用顯示** 服務名稱（例如 "Battery Service Device"）
- **預設顯示** "Unknown Device"（無任何資訊時）

#### 斷線改進
- **正確清理** GATT Session 資源
- **移除訂閱** 通知的事件處理器
- **清空容器** services 和 characteristics
- **修復問題** 斷線後裝置仍顯示連線狀態

---

## 常見問題

### 編譯問題

**Q: 找不到 Windows SDK**  
A: 確認已安裝 Windows SDK 10.0.19041.0 或更新版本。使用 Visual Studio Installer 安裝「Windows 10 SDK」元件。

**Q: WinRT 編譯錯誤**  
A: 檢查以下設定：
- CMakeLists.txt 中的 `WINVER` 和 `_WIN32_WINNT` 定義為 `0x0A00`
- 確認使用 C++17 標準 (`/std:c++17`)
- 確認啟用 C++/WinRT 支援 (`/await`)

**Q: 編譯時出現 "windowsapp.lib not found"**  
A: 確認 Visual Studio 已安裝「通用 Windows 平台開發」工作負載。

### 執行問題

**Q: 找不到 ble_api.dll**  
A: 確保 DLL 與執行檔在同一目錄，或將 DLL 路徑加入系統 PATH 環境變數。

**Q: 配對失敗**  
A: 
- 檢查裝置是否已在 Windows 設定中配對
- 若已配對，請先「移除裝置」再重新連線
- 確認輸入正確的 PIN 碼（通常是 000000 或 123456）

**Q: 連線後無法斷線**  
A: 確認已更新到 v1.1.0 版本。該版本修復了 GATT Session 清理問題。

**Q: 掃描不到裝置**  
A:
- 確認裝置正在廣播
- 確認 Windows 藍牙已啟用
- 嘗試使用 Active 模式掃描（可獲取更多資訊）
- 檢查 Windows 隱私設定是否允許應用程式存取藍牙

**Q: 無法讀取裝置名稱**  
A:
- 使用 Active 掃描模式（可發送 Scan Request）
- 某些裝置僅在 Scan Response 中回傳 LocalName
- 檢查裝置是否正確設定廣播名稱

---

## 技術細節

### Advertisement/Scan Response 配對機制

Windows BLE API 會分別接收 Advertisement 和 Scan Response 兩個封包。本函式庫實作了自動配對機制：

1. **接收 Advertisement**：記錄裝置地址、RSSI、廠商資料、服務 UUID
2. **等待 Scan Response**：延遲 200ms 以接收可能的 Scan Response
3. **配對封包**：將 Scan Response 的 LocalName 與 Advertisement 資料合併
4. **觸發回調**：僅在配對完成後觸發 `ScanFoundCallback`

### 裝置名稱顯示優先順序

```
1. LocalName (從 Scan Response)
   ↓ 無 LocalName
2. 廠商名稱 (從 Manufacturer Data)
   ↓ 無廠商資料
3. 服務名稱 (從 Service UUID)
   ↓ 無服務資料
4. "Unknown Device"
```

### Active vs Passive 掃描

| 模式 | 發送 Scan Request | LocalName | 功耗 | 適用場景 |
|------|-------------------|-----------|------|----------|
| Active | ✅ | 通常可取得 | 較高 | 需要完整裝置資訊 |
| Passive | ❌ | 可能無法取得 | 較低 | 省電、背景掃描 |

---

## 授權

本專案以 [MIT License](LICENSE) 授權發布。

---

## 聯絡資訊

如有問題或建議，請聯絡開發團隊。

---
