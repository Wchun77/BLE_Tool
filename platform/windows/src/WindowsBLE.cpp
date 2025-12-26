#include "WindowsBLE.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#include <set>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace BLE {

struct WindowsBLE::Impl {
    BluetoothLEAdvertisementWatcher watcher{nullptr};
    BluetoothLEDevice device{nullptr};
    std::map<std::string, GattDeviceService> services;
    std::map<std::string, GattCharacteristic> characteristics;
    std::set<std::string> seenDevices;  // 記錄已掃描的裝置地址
    bool isConnected = false;
    
    Impl() = default;
    ~Impl() = default;
};

WindowsBLE::WindowsBLE()
    : pImpl_(std::make_unique<Impl>())
{
}

WindowsBLE::~WindowsBLE() {
    shutdown();
}

bool WindowsBLE::initialize() {
    try {
        init_apartment();
        return true;
    } catch (const hresult_error& ex) {
        setError("WinRT initialization failed: " + to_string(ex.message()));
        return false;
    }
}

void WindowsBLE::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    stopScan();
    disconnect();
    
    pImpl_->services.clear();
    pImpl_->characteristics.clear();
    pImpl_->seenDevices.clear();
}

// 輔助函數：取得廠商名稱
static std::string getManufacturerName(uint16_t companyId) {
    // 常見廠商 ID 對照表
    switch (companyId) {
        case 0x004C: return "Apple";
        case 0x0006: return "Microsoft";
        case 0x0075: return "Samsung";
        case 0x00E0: return "Google";
        case 0x0087: return "Garmin";
        case 0x0157: return "Fitbit";
        case 0x02E5: return "Xiaomi";
        case 0x0590: return "Logitech";
        case 0x0499: return "Ruuvi";
        case 0x0171: return "Nordic Semiconductor";
        case 0x0822: return "Espressif";  // ESP32
        default: {
            char name[32];
            sprintf_s(name, sizeof(name), "Vendor 0x%04X", companyId);
            return name;
        }
    }
}

// 輔助函數：GUID 轉字串
static std::string guidToString(const winrt::guid& uuid) {
    char str[37];
    sprintf_s(str, sizeof(str), 
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid.Data1, uuid.Data2, uuid.Data3,
        uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
        uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);
    return str;
}

// 輔助函數：識別常見服務
static std::string getServiceName(const std::string& uuid) {
    if (uuid.find("0000180f") == 0) return "Battery Service";
    if (uuid.find("0000180a") == 0) return "Device Information";
    if (uuid.find("00001800") == 0) return "Generic Access";
    if (uuid.find("00001801") == 0) return "Generic Attribute";
    if (uuid.find("0000180d") == 0) return "Heart Rate";
    if (uuid.find("00001816") == 0) return "Cycling Speed and Cadence";
    if (uuid.find("0000181c") == 0) return "User Data";
    return "";
}

bool WindowsBLE::startScan(int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    (void)timeoutMs; // 暫時不處理 timeout
    
    try {
        // 清空已掃描裝置記錄
        pImpl_->seenDevices.clear();
        
        // 建立 Advertisement Watcher
        pImpl_->watcher = BluetoothLEAdvertisementWatcher();
        
        // 設定掃描模式（主動掃描以取得更多資訊）
        pImpl_->watcher.ScanningMode(BluetoothLEScanningMode::Active);
        
        // 設定接收回調
        pImpl_->watcher.Received([this](
            BluetoothLEAdvertisementWatcher,
            BluetoothLEAdvertisementReceivedEventArgs args) {
            
            DeviceInfo info;
            
            // === 1. 地址 ===
            uint64_t addr = args.BluetoothAddress();
            char addrStr[18];
            sprintf_s(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                (unsigned char)((addr >> 40) & 0xFF),
                (unsigned char)((addr >> 32) & 0xFF),
                (unsigned char)((addr >> 24) & 0xFF),
                (unsigned char)((addr >> 16) & 0xFF),
                (unsigned char)((addr >> 8) & 0xFF),
                (unsigned char)(addr & 0xFF)
            );
            info.address = addrStr;
            
            // === 過濾重複裝置 ===
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (pImpl_->seenDevices.count(info.address) > 0) {
                    return; // 已經掃描過，跳過
                }
                pImpl_->seenDevices.insert(info.address);
            }
            
            // === 2. RSSI ===
            info.rssi = args.RawSignalStrengthInDBm();
            
            // === 3. 裝置名稱 ===
            auto localName = args.Advertisement().LocalName();
            if (!localName.empty()) {
                info.name = to_string(localName);
                info.hasName = true;
            } else {
                info.hasName = false;
                info.name = "Unknown Device";
            }
            
            // === 4. 廠商資訊 ===
            auto manufacturerData = args.Advertisement().ManufacturerData();
            if (manufacturerData.Size() > 0) {
                auto data = manufacturerData.GetAt(0);
                info.manufacturerId = data.CompanyId();
                
                // 如果沒有名稱，用廠商名稱代替
                if (!info.hasName) {
                    info.name = getManufacturerName(info.manufacturerId);
                }
            }
            
            // === 5. 服務 UUID ===
            auto serviceUuids = args.Advertisement().ServiceUuids();
            for (uint32_t i = 0; i < serviceUuids.Size(); i++) {
                auto uuid = serviceUuids.GetAt(i);
                std::string uuidStr = guidToString(uuid);
                info.serviceUuids.push_back(uuidStr);
                
                // 如果沒有名稱，嘗試用服務名稱
                if (!info.hasName) {
                    std::string serviceName = getServiceName(uuidStr);
                    if (!serviceName.empty()) {
                        info.name = serviceName + " Device";
                        break;
                    }
                }
            }
            
            // === 回調 ===
            if (scanCallback_) {
                scanCallback_(info);
            }
        });
        
        // 開始掃描
        pImpl_->watcher.Start();
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Start scan failed: " + to_string(ex.message()));
        return false;
    }
}

void WindowsBLE::stopScan() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pImpl_->watcher) {
        try {
            pImpl_->watcher.Stop();
            pImpl_->watcher = nullptr;
        } catch (...) {
            // 忽略停止時的錯誤
        }
    }
}

void WindowsBLE::setScanCallback(ScanCallback callback) {
    scanCallback_ = callback;
}

bool WindowsBLE::connect(const std::string& deviceAddress) {
    (void)deviceAddress;
    setError("Connect not implemented yet");
    return false;
}

bool WindowsBLE::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pImpl_->device) {
        try {
            pImpl_->device.Close();
            pImpl_->device = nullptr;
            pImpl_->isConnected = false;
            
            if (connectionCallback_) {
                connectionCallback_(false, "Disconnected");
            }
        } catch (...) {
            // 忽略錯誤
        }
    }
    
    return true;
}

bool WindowsBLE::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pImpl_->isConnected;
}

void WindowsBLE::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = callback;
}

bool WindowsBLE::discoverServices() {
    setError("Discover services not implemented yet");
    return false;
}

std::vector<std::string> WindowsBLE::getServices() {
    return {};
}

std::vector<std::string> WindowsBLE::getCharacteristics(const std::string& serviceUuid) {
    (void)serviceUuid;
    return {};
}

bool WindowsBLE::writeData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    const uint8_t* data,
    size_t length,
    bool withResponse)
{
    (void)serviceUuid;
    (void)charUuid;
    (void)data;
    (void)length;
    (void)withResponse;
    setError("Write data not implemented yet");
    return false;
}

bool WindowsBLE::readData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    std::vector<uint8_t>& outData)
{
    (void)serviceUuid;
    (void)charUuid;
    (void)outData;
    setError("Read data not implemented yet");
    return false;
}

bool WindowsBLE::subscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid,
    DataCallback callback)
{
    (void)serviceUuid;
    (void)charUuid;
    (void)callback;
    setError("Subscribe notify not implemented yet");
    return false;
}

bool WindowsBLE::unsubscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid)
{
    (void)serviceUuid;
    (void)charUuid;
    setError("Unsubscribe notify not implemented yet");
    return false;
}

std::string WindowsBLE::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void WindowsBLE::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = error;
}

// 工廠函數實作
std::unique_ptr<IBLEBackend> createPlatformBackend() {
    return std::make_unique<WindowsBLE>();
}

} // namespace BLE