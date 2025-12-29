#include "WindowsBLE.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <set>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <thread>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;
using namespace Windows::Devices::Enumeration;

namespace BLE {

struct WindowsBLE::Impl {
    BluetoothLEAdvertisementWatcher watcher{nullptr};
    BluetoothLEDevice device{nullptr};
    std::unordered_map<std::string, GattDeviceService> services;
    std::unordered_map<std::string, GattCharacteristic> characteristics;
    std::set<std::string> seenDevices;
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
        // 嘗試預設初始化（通常是 MTA）
        init_apartment();
        return true;
    } catch (const hresult_error& ex) {
        // 如果執行緒已經初始化過 COM（例如 WinForms 的 STA）
        if (ex.code() == 0x80010106) {  // RPC_E_CHANGED_MODE
            // 執行緒已經在某個 apartment 模式，直接使用它
            return true;
        }
        
        // 其他錯誤才真的失敗
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

// ============================================================================
// 輔助函數（static，只在此檔案內使用）
// ============================================================================

// 取得廠商名稱
static std::string getManufacturerName(uint16_t companyId) {
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
        case 0x0822: return "Espressif";
        default: {
            char name[32];
            sprintf_s(name, sizeof(name), "Vendor 0x%04X", companyId);
            return name;
        }
    }
}

// GUID 轉字串
static std::string guidToString(const winrt::guid& uuid) {
    char str[37];
    sprintf_s(str, sizeof(str), 
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid.Data1, uuid.Data2, uuid.Data3,
        uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
        uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);
    return str;
}

// 字串轉 GUID
static winrt::guid stringToGuid(const std::string& uuidStr) {
    // 移除所有 '-'
    std::string clean = uuidStr;
    clean.erase(std::remove(clean.begin(), clean.end(), '-'), clean.end());
    
    // 確保長度正確
    if (clean.length() != 32) {
        throw std::invalid_argument("Invalid UUID string");
    }
    
    // 解析
    winrt::guid uuid;
    sscanf_s(clean.c_str(), 
        "%8lx%4hx%4hx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
        &uuid.Data1, &uuid.Data2, &uuid.Data3,
        &uuid.Data4[0], &uuid.Data4[1], &uuid.Data4[2], &uuid.Data4[3],
        &uuid.Data4[4], &uuid.Data4[5], &uuid.Data4[6], &uuid.Data4[7]);
    
    return uuid;
}

// 識別常見服務
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

// ============================================================================
// WindowsBLE 成員函數實作
// ============================================================================

// 地址字串轉 uint64_t
uint64_t WindowsBLE::addressStringToUint64(const std::string& address) {
    // "XX:XX:XX:XX:XX:XX" -> uint64_t
    uint64_t addr = 0;
    int values[6];
    
    if (sscanf_s(address.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
                 &values[0], &values[1], &values[2], 
                 &values[3], &values[4], &values[5]) != 6) {
        return 0;
    }
    
    for (int i = 0; i < 6; i++) {
        addr = (addr << 8) | (values[i] & 0xFF);
    }
    
    return addr;
}

bool WindowsBLE::startScan(int timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    (void)timeoutMs;
    
    try {
        pImpl_->seenDevices.clear();
        pImpl_->watcher = BluetoothLEAdvertisementWatcher();
        pImpl_->watcher.ScanningMode(BluetoothLEScanningMode::Active);
        
        pImpl_->watcher.Received([this](
            BluetoothLEAdvertisementWatcher,
            BluetoothLEAdvertisementReceivedEventArgs args) {
            
            DeviceInfo info;
            
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
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (pImpl_->seenDevices.count(info.address) > 0) {
                    return;
                }
                pImpl_->seenDevices.insert(info.address);
            }
            
            info.rssi = args.RawSignalStrengthInDBm();
            
            auto localName = args.Advertisement().LocalName();
            if (!localName.empty()) {
                info.name = to_string(localName);
                info.hasName = true;
            } else {
                info.hasName = false;
                info.name = "Unknown Device";
            }
            
            auto manufacturerData = args.Advertisement().ManufacturerData();
            if (manufacturerData.Size() > 0) {
                auto data = manufacturerData.GetAt(0);
                info.manufacturerId = data.CompanyId();
                
                if (!info.hasName) {
                    info.name = getManufacturerName(info.manufacturerId);
                }
            }
            
            auto serviceUuids = args.Advertisement().ServiceUuids();
            for (uint32_t i = 0; i < serviceUuids.Size(); i++) {
                auto uuid = serviceUuids.GetAt(i);
                std::string uuidStr = guidToString(uuid);
                info.serviceUuids.push_back(uuidStr);
                
                if (!info.hasName) {
                    std::string serviceName = getServiceName(uuidStr);
                    if (!serviceName.empty()) {
                        info.name = serviceName + " Device";
                        break;
                    }
                }
            }
            
            if (scanCallback_) {
                scanCallback_(info);
            }
        });
        
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
        }
    }
}

void WindowsBLE::setScanCallback(ScanCallback callback) {
    scanCallback_ = callback;
}

bool WindowsBLE::connect(const std::string& deviceAddress) {
    try {
        uint64_t addr = addressStringToUint64(deviceAddress);
        if (addr == 0) {
            setError("Invalid device address format");
            return false;
        }
        
        // 1. 取得裝置
        std::cout << "  Getting device..." << std::endl;
        auto asyncOp = BluetoothLEDevice::FromBluetoothAddressAsync(addr);
        pImpl_->device = asyncOp.get();
        
        if (!pImpl_->device) {
            setError("Failed to get BluetoothLEDevice");
            return false;
        }
        
        // 2. 檢查配對狀態
        auto deviceInfo = pImpl_->device.DeviceInformation();
        auto pairing = deviceInfo.Pairing();
        
        std::cout << "  Checking pairing status..." << std::endl;
        
        if (!pairing.IsPaired()) {
            std::cout << "  Device not paired. Initiating pairing..." << std::endl;
            std::cout << "  >> Please enter PIN: 000000 in the Windows dialog" << std::endl;
            
            // 3. 發起配對（會觸發 Windows 對話框）
            auto pairResult = pairing.PairAsync().get();
            
            if (pairResult.Status() == DevicePairingResultStatus::Paired) {
                std::cout << "  Pairing successful!" << std::endl;
            } else if (pairResult.Status() == DevicePairingResultStatus::AlreadyPaired) {
                std::cout << "  Already paired" << std::endl;
            } else {
                std::string errorMsg = "Pairing failed: ";
                switch (pairResult.Status()) {
                    case DevicePairingResultStatus::PairingCanceled:
                        errorMsg += "User canceled";
                        break;
                    case DevicePairingResultStatus::AuthenticationTimeout:
                        errorMsg += "Authentication timeout";
                        break;
                    case DevicePairingResultStatus::AuthenticationFailure:
                        errorMsg += "Authentication failure (wrong PIN?)";
                        break;
                    case DevicePairingResultStatus::ConnectionRejected:
                        errorMsg += "Connection rejected";
                        break;
                    case DevicePairingResultStatus::Failed:
                        errorMsg += "Failed";
                        break;
                    default:
                        errorMsg += "Unknown error";
                        break;
                }
                setError(errorMsg);
                return false;
            }
        } else {
            std::cout << "  Device already paired" << std::endl;
        }
        
        // 4. 主動建立連線（透過 GATT 操作）
        std::cout << "  Establishing connection..." << std::endl;
        
        try {
            // 嘗試獲取 GATT 服務來觸發連線
            auto servicesResult = pImpl_->device.GetGattServicesAsync(
                BluetoothCacheMode::Uncached  // 強制建立新連線
            ).get();
            
            if (servicesResult.Status() != GattCommunicationStatus::Success) {
                setError("Failed to connect to device (GATT error)");
                return false;
            }
            
            std::cout << "  Connection established!" << std::endl;
            
        } catch (const hresult_error& ex) {
            setError("Failed to establish connection: " + to_string(ex.message()));
            return false;
        }
        
        // 5. 確認連線狀態
        auto status = pImpl_->device.ConnectionStatus();
        pImpl_->isConnected = (status == BluetoothConnectionStatus::Connected);
        
        if (!pImpl_->isConnected) {
            setError("Device did not connect");
            return false;
        }
        
        std::cout << "  Connected!" << std::endl;
        
        if (connectionCallback_) {
            connectionCallback_(pImpl_->isConnected, "Connected");
        }
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Connect failed: " + to_string(ex.message()));
        return false;
    }
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
    try {
        if (!pImpl_->device) {
            setError("Not connected");
            return false;
        }
        
        pImpl_->services.clear();
        pImpl_->characteristics.clear();
        
        // 取得所有 GATT 服務
        auto servicesResult = pImpl_->device.GetGattServicesAsync().get();
        
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
            setError("Failed to discover services");
            return false;
        }
        
        auto services = servicesResult.Services();
        for (uint32_t i = 0; i < services.Size(); i++) {
            auto service = services.GetAt(i);
            std::string uuid = guidToString(service.Uuid());
            
            // 使用 emplace 而不是 operator[]
            pImpl_->services.emplace(uuid, service);
            
            auto charsResult = service.GetCharacteristicsAsync().get();
            if (charsResult.Status() == GattCommunicationStatus::Success) {
                auto chars = charsResult.Characteristics();
                for (uint32_t j = 0; j < chars.Size(); j++) {
                    auto characteristic = chars.GetAt(j);
                    std::string charUuid = guidToString(characteristic.Uuid());
                    
                    // 使用 emplace
                    pImpl_->characteristics.emplace(uuid + ":" + charUuid, characteristic);
                }
            }
        }
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Discover services failed: " + to_string(ex.message()));
        return false;
    }
}

std::vector<std::string> WindowsBLE::getServices() {
    std::vector<std::string> result;
    for (const auto& pair : pImpl_->services) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<std::string> WindowsBLE::getCharacteristics(const std::string& serviceUuid) {
    std::vector<std::string> result;
    std::string prefix = serviceUuid + ":";
    
    for (const auto& pair : pImpl_->characteristics) {
        if (pair.first.find(prefix) == 0) {
            std::string charUuid = pair.first.substr(prefix.length());
            result.push_back(charUuid);
        }
    }
    
    return result;
}

bool WindowsBLE::writeData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    const uint8_t* data,
    size_t length,
    bool withResponse)
{
    try {
        std::string key = serviceUuid + ":" + charUuid;
        auto it = pImpl_->characteristics.find(key);
        
        if (it == pImpl_->characteristics.end()) {
            setError("Characteristic not found");
            return false;
        }
        
        auto characteristic = it->second;
        
        // 建立資料緩衝區
        DataWriter writer;
        writer.WriteBytes(array_view<const uint8_t>(data, data + length));
        auto buffer = writer.DetachBuffer();
        
        // 寫入選項
        auto writeOption = withResponse ? 
            GattWriteOption::WriteWithResponse : 
            GattWriteOption::WriteWithoutResponse;
        
        // 寫入資料
        auto writeResult = characteristic.WriteValueWithResultAsync(buffer, writeOption).get();
        
        if (writeResult.Status() != GattCommunicationStatus::Success) {
            setError("Write failed");
            return false;
        }
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Write data failed: " + to_string(ex.message()));
        return false;
    }
}

bool WindowsBLE::readData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    std::vector<uint8_t>& outData)
{
    try {
        std::string key = serviceUuid + ":" + charUuid;
        auto it = pImpl_->characteristics.find(key);
        
        if (it == pImpl_->characteristics.end()) {
            setError("Characteristic not found");
            return false;
        }
        
        auto characteristic = it->second;
        auto readResult = characteristic.ReadValueAsync().get();
        
        if (readResult.Status() != GattCommunicationStatus::Success) {
            setError("Read failed");
            return false;
        }
        
        auto buffer = readResult.Value();
        DataReader reader = DataReader::FromBuffer(buffer);
        
        outData.resize(buffer.Length());
        reader.ReadBytes(outData);
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Read data failed: " + to_string(ex.message()));
        return false;
    }
}

bool WindowsBLE::subscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid,
    DataCallback callback)
{
    try {
        std::string key = serviceUuid + ":" + charUuid;
        auto it = pImpl_->characteristics.find(key);
        
        if (it == pImpl_->characteristics.end()) {
            setError("Characteristic not found");
            return false;
        }
        
        auto characteristic = it->second;
        
        // 儲存回調
        notifyCallbacks_[key] = callback;
        
        // 註冊值變化事件
        characteristic.ValueChanged([this, key](
            GattCharacteristic const&,
            GattValueChangedEventArgs args) {
            
            auto buffer = args.CharacteristicValue();
            DataReader reader = DataReader::FromBuffer(buffer);
            
            std::vector<uint8_t> data(buffer.Length());
            reader.ReadBytes(data);
            
            if (notifyCallbacks_.count(key) > 0 && notifyCallbacks_[key]) {
                notifyCallbacks_[key](data.data(), data.size());
            }
        });
        
        // 寫入 CCCD 啟用通知
        auto result = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify
        ).get();
        
        if (result != GattCommunicationStatus::Success) {
            setError("Failed to enable notifications");
            return false;
        }
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Subscribe notify failed: " + to_string(ex.message()));
        return false;
    }
}

bool WindowsBLE::unsubscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid)
{
    try {
        std::string key = serviceUuid + ":" + charUuid;
        auto it = pImpl_->characteristics.find(key);
        
        if (it == pImpl_->characteristics.end()) {
            return false;
        }
        
        auto characteristic = it->second;
        
        // 停用通知
        characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::None
        ).get();
        
        // 移除回調
        notifyCallbacks_.erase(key);
        
        return true;
        
    } catch (...) {
        return false;
    }
}

std::string WindowsBLE::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void WindowsBLE::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = error;
}

std::unique_ptr<IBLEBackend> createPlatformBackend() {
    return std::make_unique<WindowsBLE>();
}

} // namespace BLE