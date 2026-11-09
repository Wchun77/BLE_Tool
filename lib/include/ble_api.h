#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// DLL 導出宏
#ifdef _WIN32
    #ifdef BLE_API_EXPORTS
        #define BLE_API __declspec(dllexport)
    #else
        #define BLE_API __declspec(dllimport)
    #endif
#else
    #define BLE_API __attribute__((visibility("default")))
#endif

// 句柄類型
typedef void* BLE_HANDLE;

// 掃描模式
typedef enum {
    BLE_SCAN_MODE_ACTIVE = 0,
    BLE_SCAN_MODE_PASSIVE = 1
} BLE_ScanMode;

// 回調函數類型
typedef void (*BLE_ScanFoundCallback)(
    const char* name,
    const char* address,
    int16_t rssi,
    int hasName,
    uint16_t manufacturerId,
    const char* serviceUuids,
    uint64_t timestamp,
    void* userData);

typedef void (*BLE_ScanUpdatedCallback)(
    const char* name,
    const char* address,
    int16_t rssi,
    int hasName,
    uint16_t manufacturerId,
    const char* serviceUuids,
    uint64_t timestamp,
    void* userData);

typedef void (*BLE_DataCallback)(
    const uint8_t* data,
    size_t length,
    void* userData);

typedef void (*BLE_ConnectionCallback)(
    int connected,
    const char* message,
    void* userData);

typedef void (*BLE_ErrorCallback)(
    const char* error,
    void* userData);

// 返回值定義
#define BLE_SUCCESS 0
#define BLE_ERROR -1

// === 初始化/清理 ===
BLE_API BLE_HANDLE BLE_Create(void);
BLE_API void BLE_Destroy(BLE_HANDLE handle);
BLE_API int BLE_Initialize(BLE_HANDLE handle);
BLE_API void BLE_Shutdown(BLE_HANDLE handle);
BLE_API int BLE_IsInitialized(BLE_HANDLE handle);

// === 掃描 ===

BLE_API int BLE_StartScan(
    BLE_HANDLE handle,
    int timeoutMs,
    BLE_ScanMode mode,
    BLE_ScanFoundCallback foundCallback,
    BLE_ScanUpdatedCallback updatedCallback,
    void* userData);

BLE_API void BLE_StopScan(BLE_HANDLE handle);


// === 連線 ===
BLE_API int BLE_Connect(BLE_HANDLE handle, const char* deviceAddress);
BLE_API int BLE_Disconnect(BLE_HANDLE handle);
BLE_API int BLE_IsConnected(BLE_HANDLE handle);
BLE_API void BLE_SetConnectionCallback(
    BLE_HANDLE handle,
    BLE_ConnectionCallback callback,
    void* userData);

// === 配對 ===
// 設定配對碼 (例如 "000000")。設定後 BLE_Connect 遇到未配對的裝置會自己帶入配對碼配對，
// 不需要使用者先到 Windows 設定手動配對。傳 NULL 或空字串 = 恢復原本行為 (由系統處理配對)。
BLE_API void BLE_SetPairingPin(BLE_HANDLE handle, const char* pin);

// 查詢是否已配對：1 = 已配對、0 = 未配對、BLE_ERROR = 查詢失敗
BLE_API int BLE_IsPaired(BLE_HANDLE handle, const char* deviceAddress);

// 移除配對 (同時清掉 Windows 對這台裝置的 GATT 快取)。已連線時會先斷線。
BLE_API int BLE_Unpair(BLE_HANDLE handle, const char* deviceAddress);

// === 服務/特性 ===
BLE_API int BLE_DiscoverServices(BLE_HANDLE handle);

// === 資料讀寫 ===
BLE_API int BLE_WriteData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    const uint8_t* data,
    size_t length,
    int withResponse);

BLE_API int BLE_ReadData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    uint8_t* buffer,
    size_t* length);

// === 通知訂閱 ===
BLE_API int BLE_SubscribeNotify(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    BLE_DataCallback callback,
    void* userData);

BLE_API int BLE_UnsubscribeNotify(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid);

// === 錯誤處理 ===
BLE_API const char* BLE_GetLastError(BLE_HANDLE handle);
BLE_API void BLE_SetErrorCallback(
    BLE_HANDLE handle,
    BLE_ErrorCallback callback,
    void* userData);

// === 版本資訊 ===
BLE_API const char* BLE_GetVersion(void);

#ifdef __cplusplus
}
#endif