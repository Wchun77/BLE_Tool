#include "ble_api.h"
#include "BLEManager.h"
#include <string>
#include <map>
#include <cstring>
#include <memory>

using namespace BLE;

// 內部上下文
struct BLE_Context {
    BLEManager manager;
    std::string lastError;

    BLE_ScanFoundCallback foundCallback = nullptr;
    BLE_ScanUpdatedCallback updatedCallback = nullptr;
    void* scanUserData = nullptr;

    BLE_ConnectionCallback connCallback = nullptr;
    void* connUserData = nullptr;

    BLE_ErrorCallback errorCallback = nullptr;
    void* errorUserData = nullptr;

    std::map<std::string, BLE_DataCallback> dataCallbacks;
    std::map<std::string, void*> dataUserData;

    BLE_Context() = default;
};

// handle 實際上是 shared_ptr 的裸指標，用 wrapper 管理
struct BLE_HandleWrapper {
    std::shared_ptr<BLE_Context> ctx;
};

// === 初始化/清理 ===

BLE_HANDLE BLE_Create(void) {
    try {
        auto wrapper = new BLE_HandleWrapper();
        wrapper->ctx = std::make_shared<BLE_Context>();
        return wrapper;
    } catch (...) {
        return nullptr;
    }
}

void BLE_Destroy(BLE_HANDLE handle) {
    if (handle) {
        auto wrapper = static_cast<BLE_HandleWrapper*>(handle);
        // shared_ptr 的 refcount 降低，但 lambda 裡的 weak_ptr 仍然安全
        delete wrapper;
    }
}

// 內部輔助：從 handle 取得 shared_ptr
static std::shared_ptr<BLE_Context> getCtx(BLE_HANDLE handle) {
    if (!handle) return nullptr;
    return static_cast<BLE_HandleWrapper*>(handle)->ctx;
}

int BLE_Initialize(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return BLE_ERROR;

    try {
        ctx->manager.setErrorCallback([weak = std::weak_ptr<BLE_Context>(ctx)](const std::string& error) {
            auto ctx = weak.lock();
            if (!ctx) return;
            ctx->lastError = error;
            if (ctx->errorCallback) {
                ctx->errorCallback(error.c_str(), ctx->errorUserData);
            }
        });

        return ctx->manager.initialize() ? BLE_SUCCESS : BLE_ERROR;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

void BLE_Shutdown(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return;

    try {
        ctx->manager.shutdown();
    } catch (...) {}
}

int BLE_IsInitialized(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return 0;
    return ctx->manager.isInitialized() ? 1 : 0;
}

// === 掃描 ===

int BLE_StartScan(
    BLE_HANDLE handle,
    int timeoutMs,
    BLE_ScanMode mode,
    BLE_ScanFoundCallback foundCallback,
    BLE_ScanUpdatedCallback updatedCallback,
    void* userData)
{
    auto ctx = getCtx(handle);
    if (!ctx) return BLE_ERROR;

    try {
        ctx->foundCallback = foundCallback;
        ctx->updatedCallback = updatedCallback;
        ctx->scanUserData = userData;

        ScanMode cppMode = (mode == BLE_SCAN_MODE_ACTIVE) ?
            ScanMode::Active : ScanMode::Passive;

        ScanFoundCallback cppFoundCallback = nullptr;
        if (foundCallback) {
            cppFoundCallback = [weak = std::weak_ptr<BLE_Context>(ctx)](const DeviceInfo& dev) {
                auto ctx = weak.lock();
                if (!ctx) return;

                std::string uuidsStr;
                for (size_t i = 0; i < dev.serviceUuids.size(); i++) {
                    if (i > 0) uuidsStr += ", ";
                    uuidsStr += dev.serviceUuids[i];
                }

                ctx->foundCallback(
                    dev.name.c_str(),
                    dev.address.c_str(),
                    dev.rssi,
                    dev.hasName ? 1 : 0,
                    dev.manufacturerId,
                    uuidsStr.c_str(),
                    dev.timestamp,
                    ctx->scanUserData
                );
            };
        }

        ScanUpdatedCallback cppUpdatedCallback = nullptr;
        if (updatedCallback) {
            cppUpdatedCallback = [weak = std::weak_ptr<BLE_Context>(ctx)](const DeviceInfo& dev) {
                auto ctx = weak.lock();
                if (!ctx) return;

                std::string uuidsStr;
                for (size_t i = 0; i < dev.serviceUuids.size(); i++) {
                    if (i > 0) uuidsStr += ", ";
                    uuidsStr += dev.serviceUuids[i];
                }

                ctx->updatedCallback(
                    dev.name.c_str(),
                    dev.address.c_str(),
                    dev.rssi,
                    dev.hasName ? 1 : 0,
                    dev.manufacturerId,
                    uuidsStr.c_str(),
                    dev.timestamp,
                    ctx->scanUserData
                );
            };
        }

        return ctx->manager.startScan(
            timeoutMs,
            cppMode,
            cppFoundCallback,
            cppUpdatedCallback
        ) ? BLE_SUCCESS : BLE_ERROR;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

void BLE_StopScan(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return;
    ctx->manager.stopScan();
}

// === 連線 ===

int BLE_Connect(BLE_HANDLE handle, const char* deviceAddress) {
    auto ctx = getCtx(handle);
    if (!ctx || !deviceAddress) return BLE_ERROR;

    try {
        if (ctx->connCallback) {
            ctx->manager.setConnectionCallback([weak = std::weak_ptr<BLE_Context>(ctx)](bool connected, const std::string& msg) {
                auto ctx = weak.lock();
                if (!ctx) return;
                ctx->connCallback(connected ? 1 : 0, msg.c_str(), ctx->connUserData);
            });
        }

        if (ctx->manager.connect(deviceAddress)) {
            return BLE_SUCCESS;
        }
        // 後端的錯誤原本不會傳到 lastError，C# 端 GetLastError 會拿到空字串
        ctx->lastError = ctx->manager.getLastError();
        return BLE_ERROR;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

// === 配對 ===

void BLE_SetPairingPin(BLE_HANDLE handle, const char* pin) {
    auto ctx = getCtx(handle);
    if (!ctx) return;

    try {
        ctx->manager.setPairingPin(pin ? pin : "");
    } catch (...) {}
}

int BLE_IsPaired(BLE_HANDLE handle, const char* deviceAddress) {
    auto ctx = getCtx(handle);
    if (!ctx || !deviceAddress) return BLE_ERROR;

    try {
        int r = ctx->manager.isPaired(deviceAddress);
        if (r < 0) {
            ctx->lastError = ctx->manager.getLastError();
            return BLE_ERROR;
        }
        return r;
    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

int BLE_Unpair(BLE_HANDLE handle, const char* deviceAddress) {
    auto ctx = getCtx(handle);
    if (!ctx || !deviceAddress) return BLE_ERROR;

    try {
        if (ctx->manager.unpair(deviceAddress)) {
            return BLE_SUCCESS;
        }
        ctx->lastError = ctx->manager.getLastError();
        return BLE_ERROR;
    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

int BLE_Disconnect(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return BLE_ERROR;
    return ctx->manager.disconnect() ? BLE_SUCCESS : BLE_ERROR;
}

int BLE_IsConnected(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return 0;
    return ctx->manager.isConnected() ? 1 : 0;
}

void BLE_SetConnectionCallback(BLE_HANDLE handle, BLE_ConnectionCallback callback, void* userData) {
    auto ctx = getCtx(handle);
    if (!ctx) return;
    ctx->connCallback = callback;
    ctx->connUserData = userData;
}

// === 服務/特性 ===

int BLE_DiscoverServices(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return BLE_ERROR;
    return ctx->manager.discoverServices() ? BLE_SUCCESS : BLE_ERROR;
}

// === 資料讀寫 ===

int BLE_WriteData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    const uint8_t* data,
    size_t length,
    int withResponse)
{
    auto ctx = getCtx(handle);
    if (!ctx || !serviceUuid || !charUuid || !data) return BLE_ERROR;

    try {
        return ctx->manager.writeData(
            serviceUuid, charUuid, data, length, withResponse != 0
        ) ? BLE_SUCCESS : BLE_ERROR;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

int BLE_ReadData(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    uint8_t* buffer,
    size_t* length)
{
    auto ctx = getCtx(handle);
    if (!ctx || !serviceUuid || !charUuid || !buffer || !length) return BLE_ERROR;

    try {
        std::vector<uint8_t> data;

        if (!ctx->manager.readData(serviceUuid, charUuid, data)) {
            return BLE_ERROR;
        }

        size_t copyLen = (std::min)(*length, data.size());
        std::memcpy(buffer, data.data(), copyLen);
        *length = copyLen;

        return BLE_SUCCESS;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

// === 通知訂閱 ===

int BLE_SubscribeNotify(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid,
    BLE_DataCallback callback,
    void* userData)
{
    auto ctx = getCtx(handle);
    if (!ctx || !serviceUuid || !charUuid) return BLE_ERROR;

    try {
        std::string key = std::string(serviceUuid) + ":" + charUuid;
        ctx->dataCallbacks[key] = callback;
        ctx->dataUserData[key] = userData;

        return ctx->manager.subscribeNotify(
            serviceUuid,
            charUuid,
            [weak = std::weak_ptr<BLE_Context>(ctx), key](const uint8_t* data, size_t len) {
                // weak_ptr lock 失敗代表 ctx 已被 destroy，安全跳過
                auto ctx = weak.lock();
                if (!ctx) return;

                auto it = ctx->dataCallbacks.find(key);
                if (it != ctx->dataCallbacks.end() && it->second) {
                    it->second(data, len, ctx->dataUserData[key]);
                }
            }
        ) ? BLE_SUCCESS : BLE_ERROR;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

int BLE_UnsubscribeNotify(
    BLE_HANDLE handle,
    const char* serviceUuid,
    const char* charUuid)
{
    auto ctx = getCtx(handle);
    if (!ctx || !serviceUuid || !charUuid) return BLE_ERROR;

    try {
        std::string key = std::string(serviceUuid) + ":" + charUuid;
        ctx->dataCallbacks.erase(key);
        ctx->dataUserData.erase(key);

        return ctx->manager.unsubscribeNotify(serviceUuid, charUuid) ? BLE_SUCCESS : BLE_ERROR;

    } catch (const std::exception& e) {
        ctx->lastError = e.what();
        return BLE_ERROR;
    }
}

// === 錯誤處理 ===

const char* BLE_GetLastError(BLE_HANDLE handle) {
    auto ctx = getCtx(handle);
    if (!ctx) return "Invalid handle";
    return ctx->lastError.c_str();
}

void BLE_SetErrorCallback(BLE_HANDLE handle, BLE_ErrorCallback callback, void* userData) {
    auto ctx = getCtx(handle);
    if (!ctx) return;
    ctx->errorCallback = callback;
    ctx->errorUserData = userData;
}

// === 版本資訊 ===

const char* BLE_GetVersion(void) {
    return "1.2.0";
}