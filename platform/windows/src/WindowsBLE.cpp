#include "WindowsBLE.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include "Windows.h"
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

// 廣播配對結構
struct AdvPair {
    BluetoothLEAdvertisementReceivedEventArgs adv{nullptr};
    BluetoothLEAdvertisementReceivedEventArgs scan{nullptr};
    std::chrono::steady_clock::time_point firstSeen;
    bool reported = false;
};

struct WindowsBLE::Impl {
    BluetoothLEAdvertisementWatcher watcher{nullptr};
    BluetoothLEDevice device{nullptr};
    GattSession session{nullptr};
    std::unordered_map<std::string, GattDeviceService> services;
    std::unordered_map<std::string, GattCharacteristic> characteristics;
    std::unordered_map<std::string, event_token> notifyTokens; // 儲存通知訂閱的 token
    event_token connectionStatusToken{}; // BluetoothLEDevice.ConnectionStatusChanged 的訂閱 token
    std::set<std::string> seenDevices;
    std::unordered_map<std::string, AdvPair> advPairs;  // 儲存配對
    bool isConnected = false;
    bool scanStopped = false;

    
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
    // 不在這裡加鎖，讓 disconnect() 和 stopScan() 自己管理各自的鎖
    stopScan();
    disconnect();

    std::lock_guard<std::mutex> lock(mutex_);
    pImpl_->services.clear();
    pImpl_->characteristics.clear();
    pImpl_->seenDevices.clear();
    pImpl_->advPairs.clear();
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

// 從配對中提取裝置資訊
DeviceInfo WindowsBLE::extractDeviceInfo(const std::string& address, const AdvPair& pair) {
    DeviceInfo info;
    info.address = address;
    
    // 設定 timestamp
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    info.timestamp = ms.count();
    
    // 取得 RSSI
    if (pair.scan) {
        info.rssi = pair.scan.RawSignalStrengthInDBm();
    } else if (pair.adv) {
        info.rssi = pair.adv.RawSignalStrengthInDBm();
    }
    
    // 從兩個封包中提取資料
    std::vector<BluetoothLEAdvertisementReceivedEventArgs> sources;
    if (pair.adv) sources.push_back(pair.adv);
    if (pair.scan) sources.push_back(pair.scan);
    
    // 優先從 Scan Response 取 LocalName
    std::string deviceName;
    bool hasName = false;
    
    for (const auto& source : sources) {
        if (hasName) break;
        
        // LocalName API
        auto localName = source.Advertisement().LocalName();
        if (!localName.empty()) {
            try {
                std::wstring wname(localName.c_str());
                int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (len > 1) {
                    deviceName.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &deviceName[0], len, nullptr, nullptr);
                    hasName = !deviceName.empty();
                }
            } catch (...) {}
        }
        
        // DataSections
        if (!hasName) {
            try {
                auto dataSections = source.Advertisement().DataSections();
                for (uint32_t i = 0; i < dataSections.Size(); i++) {
                    auto section = dataSections.GetAt(i);
                    uint8_t dataType = section.DataType();
                    
                    if (dataType == 0x08 || dataType == 0x09) {
                        auto buffer = section.Data();
                        auto reader = DataReader::FromBuffer(buffer);
                        
                        uint32_t bufferLen = reader.UnconsumedBufferLength();
                        if (bufferLen > 0) {
                            std::vector<uint8_t> nameBytes(bufferLen);
                            reader.ReadBytes(nameBytes);
                            deviceName = std::string(nameBytes.begin(), nameBytes.end());
                            
                            while (!deviceName.empty() && deviceName.back() == '\0') {
                                deviceName.pop_back();
                            }
                            
                            if (!deviceName.empty()) {
                                hasName = true;
                                break;
                            }
                        }
                    }
                }
            } catch (...) {}
        }
    }
    
    info.name = hasName ? deviceName : "Unknown Device";
    info.hasName = hasName;
    
    // Manufacturer Data 和 Service UUIDs
    for (const auto& source : sources) {
        if (info.manufacturerId == 0) {
            auto manufacturerData = source.Advertisement().ManufacturerData();
            if (manufacturerData.Size() > 0) {
                auto data = manufacturerData.GetAt(0);
                info.manufacturerId = data.CompanyId();
            }
        }
        
        auto serviceUuids = source.Advertisement().ServiceUuids();
        for (uint32_t i = 0; i < serviceUuids.Size(); i++) {
            auto uuid = serviceUuids.GetAt(i);
            std::string uuidStr = guidToString(uuid);
            
            if (std::find(info.serviceUuids.begin(), info.serviceUuids.end(), uuidStr) == info.serviceUuids.end()) {
                info.serviceUuids.push_back(uuidStr);
            }
        }
    }
    
    // 如果沒有名稱，用廠商或服務名稱
    if (!info.hasName) {
        if (info.manufacturerId != 0) {
            info.name = getManufacturerName(info.manufacturerId);
        } else if (!info.serviceUuids.empty()) {
            std::string serviceName = getServiceName(info.serviceUuids[0]);
            if (!serviceName.empty()) {
                info.name = serviceName + " Device";
            }
        }
    }
    
    return info;
}

bool WindowsBLE::startScan(
    int timeoutMs,
    ScanMode mode,
    ScanFoundCallback foundCallback,
    ScanUpdatedCallback updatedCallback)
{
    try {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pImpl_->scanStopped = false;
            pImpl_->advPairs.clear();
            
            // 儲存回調
            scanFoundCallback_ = foundCallback;
            scanUpdatedCallback_ = updatedCallback;
            
            pImpl_->watcher = BluetoothLEAdvertisementWatcher();
            
            // 設定掃描模式
            if (mode == ScanMode::Active) {
                pImpl_->watcher.ScanningMode(BluetoothLEScanningMode::Active);
            } else {
                pImpl_->watcher.ScanningMode(BluetoothLEScanningMode::Passive);
            }
            
            pImpl_->watcher.Received([this](
                BluetoothLEAdvertisementWatcher,
                BluetoothLEAdvertisementReceivedEventArgs args) {
                
                // 解析地址
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
                std::string address = addrStr;
                
                auto advType = args.AdvertisementType();
                bool isScanResponse = (advType == BluetoothLEAdvertisementType::ScanResponse);
                
                bool triggerFound = false;
                bool triggerUpdated = false;
                DeviceInfo info;
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    
                    auto& pair = pImpl_->advPairs[address];
                    
                    if (isScanResponse) {
                        // Scan Response
                        pair.scan = args;
                        
                        if (pair.adv && !pair.reported) {
                            // 有 Advertisement 且未報告 → 觸發 found
                            pair.reported = true;
                            info = extractDeviceInfo(address, pair);
                            triggerFound = true;
                        } else if (pair.reported) {
                            // 已報告 → 觸發 updated（如果有 callback）
                            if (scanUpdatedCallback_) {
                                info = extractDeviceInfo(address, pair);
                                triggerUpdated = true;
                            }
                        }
                    } else {
                        // Advertisement
                        bool isFirstAdv = !pair.adv;
                        
                        if (isFirstAdv) {
                            // 第一次收到 Advertisement
                            pair.firstSeen = std::chrono::steady_clock::now();
                            pair.adv = args;
                            // 不觸發，等待 Scan Response
                        } else {
                            // 重複的 Advertisement
                            pair.adv = args;
                            
                            if (pair.scan && !pair.reported) {
                                // 有 Scan Response 且未報告 → 觸發 found
                                pair.reported = true;
                                info = extractDeviceInfo(address, pair);
                                triggerFound = true;
                            } else if (!pair.reported) {
                                // 無 Scan Response，檢查超時
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - pair.firstSeen
                                ).count();
                                
                                if (elapsed >= 150) {
                                    // 超過 150ms → 觸發 found
                                    pair.reported = true;
                                    info = extractDeviceInfo(address, pair);
                                    triggerFound = true;
                                }
                            } else {
                                // 已報告 → 觸發 updated（如果有 callback）
                                if (scanUpdatedCallback_) {
                                    info = extractDeviceInfo(address, pair);
                                    triggerUpdated = true;
                                }
                            }
                        }
                    }
                }
                
                // 在鎖外觸發回調
                if (triggerFound && scanFoundCallback_) {
                    scanFoundCallback_(info);
                }
                if (triggerUpdated && scanUpdatedCallback_) {
                    scanUpdatedCallback_(info);
                }
            });
            
            pImpl_->watcher.Start();
        }
        
        // 阻塞等待
        if (timeoutMs > 0) {
            int elapsed = 0;
            while (elapsed < timeoutMs) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                elapsed += 100;
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (pImpl_->scanStopped) {
                        break;
                    }
                }
            }
            
            // 掃描結束，處理未報告的裝置
            std::vector<DeviceInfo> unreported;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                
                for (auto& [address, pair] : pImpl_->advPairs) {
                    if (!pair.reported && pair.adv) {
                        pair.reported = true;
                        unreported.push_back(extractDeviceInfo(address, pair));
                    }
                }
                
                if (pImpl_->watcher) {
                    try {
                        pImpl_->watcher.Stop();
                        pImpl_->watcher = nullptr;
                    } catch (...) {}
                }
            }
            
            // 觸發 found 回調（掃描結束時的兜底）
            if (scanFoundCallback_) {
                for (const auto& info : unreported) {
                    scanFoundCallback_(info);
                }
            }
        }
        
        return true;
        
    } catch (const hresult_error& ex) {
        setError("Start scan failed: " + to_string(ex.message()));
        return false;
    }
}

void WindowsBLE::stopScan() {
    std::lock_guard<std::mutex> lock(mutex_);

    pImpl_->scanStopped = true;
    
    if (pImpl_->watcher) {
        try {
            pImpl_->watcher.Stop();
            pImpl_->watcher = nullptr;
        } catch (...) {
        }
    }
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
        
        // 3. 建立 GATT Session（重要！）
        std::cout << "  Creating GATT session..." << std::endl;
        try {
            pImpl_->session = GattSession::FromDeviceIdAsync(pImpl_->device.BluetoothDeviceId()).get();
            if (pImpl_->session) {
                // 設定為不維持連線（讓系統管理）
                pImpl_->session.MaintainConnection(false);
                std::cout << "  GATT session created" << std::endl;
            }
        } catch (...) {
            std::cout << "  Warning: Failed to create GATT session (continuing...)" << std::endl;
        }
        
        // 4. 主動建立連線（透過 GATT 操作）
        std::cout << "  Establishing connection..." << std::endl;
        
        try {
            auto servicesResult = pImpl_->device.GetGattServicesAsync(
                BluetoothCacheMode::Uncached
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

        // 6. 訂閱連線狀態變化事件
        // isConnected() 原本只回傳我方自己維護的快取旗標,裝置斷電或訊號中斷等
        // 「非我方主動斷線」的情況不會有人更新它,導致狀態卡住。
        // ConnectionStatusChanged 是 OS 層對實際連線狀態的即時推播,訂閱後才能
        // 真正偵測到這類斷線。
        pImpl_->connectionStatusToken = pImpl_->device.ConnectionStatusChanged(
            [this](BluetoothLEDevice const& sender, IInspectable const&) {
                bool nowConnected = (sender.ConnectionStatus() == BluetoothConnectionStatus::Connected);
                ConnectionCallback callbackToFire = nullptr;

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (pImpl_->isConnected == nowConnected) {
                        return; // 狀態沒變,不重複觸發
                    }
                    pImpl_->isConnected = nowConnected;
                    callbackToFire = connectionCallback_;
                }

                if (callbackToFire) {
                    callbackToFire(nowConnected, nowConnected ? "Connected" : "Connection lost");
                }
            }
        );

        return true;
        
    } catch (const hresult_error& ex) {
        setError("Connect failed: " + to_string(ex.message()));
        return false;
    }
}

bool WindowsBLE::disconnect() {
    ConnectionCallback callbackToFire = nullptr;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!pImpl_->device) {
            return true;
        }

        try {
            std::cout << "Disconnecting..." << std::endl;

            // 1. 移除所有通知訂閱
            std::cout << "  Removing notification subscriptions..." << std::endl;
            for (auto& [key, token] : pImpl_->notifyTokens) {
                try {
                    auto it = pImpl_->characteristics.find(key);
                    if (it != pImpl_->characteristics.end()) {
                        it->second.ValueChanged(token);
                        it->second.WriteClientCharacteristicConfigurationDescriptorAsync(
                            GattClientCharacteristicConfigurationDescriptorValue::None
                        ).get();
                    }
                } catch (...) {}
            }
            pImpl_->notifyTokens.clear();
            notifyCallbacks_.clear();

            // 2. 清空 characteristics 和 services
            std::cout << "  Clearing characteristics and services..." << std::endl;
            pImpl_->characteristics.clear();
            pImpl_->services.clear();

            // 2.5 撤銷連線狀態變化訂閱
            if (pImpl_->device) {
                try {
                    pImpl_->device.ConnectionStatusChanged(pImpl_->connectionStatusToken);
                } catch (...) {}
            }

            // 3. 關閉 GATT Session
            if (pImpl_->session) {
                std::cout << "  Closing GATT session..." << std::endl;
                try {
                    pImpl_->session.Close();
                } catch (...) {}
                pImpl_->session = nullptr;
            }

            // 4. 關閉 device
            std::cout << "  Closing device..." << std::endl;
            if (pImpl_->device) {
                try {
                    pImpl_->device.Close();
                } catch (...) {}
                pImpl_->device = nullptr;
            }

            pImpl_->isConnected = false;

            std::cout << "Disconnected successfully" << std::endl;

            // mutex 釋放前只存 callback，不在這裡呼叫
            callbackToFire = connectionCallback_;

        } catch (const hresult_error& ex) {
            // 直接賦值，不呼叫 setError()，避免 recursive lock
            lastError_ = "Disconnect failed: " + to_string(ex.message());
            return false;
        } catch (...) {
            lastError_ = "Disconnect failed: unknown error";
            return false;
        }
    }  // mutex 在這裡釋放

    // mutex 釋放後才呼叫 callback，避免持鎖跨執行緒
    if (callbackToFire) {
        callbackToFire(false, "Disconnected");
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
        std::cout << "subscribeNotify key: " << key << std::endl;
        std::cout << "characteristics count: " << pImpl_->characteristics.size() << std::endl;

        auto it = pImpl_->characteristics.find(key);

        if (it == pImpl_->characteristics.end()) {
            std::cout << "  NOT FOUND, available keys:" << std::endl;
            for (const auto& pair : pImpl_->characteristics) {
                std::cout << "  - " << pair.first << std::endl;
            }
            setError("Characteristic not found");
            return false;
        }

        auto characteristic = it->second;

        // 儲存回調
        notifyCallbacks_[key] = callback;

        // 註冊值變化事件
        std::cout << "  Registering ValueChanged..." << std::endl;
        auto token = characteristic.ValueChanged([this, key](
            GattCharacteristic const&,
            GattValueChangedEventArgs args) {

            std::cout << "  ValueChanged triggered!" << std::endl;

            auto buffer = args.CharacteristicValue();
            DataReader reader = DataReader::FromBuffer(buffer);

            std::vector<uint8_t> data(buffer.Length());
            reader.ReadBytes(data);

            // 加鎖保護讀取，避免 disconnect() clear() 時的 race condition
            DataCallback callbackToFire = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = notifyCallbacks_.find(key);
                if (it != notifyCallbacks_.end() && it->second) {
                    callbackToFire = it->second;
                }
            }

            // 鎖外呼叫 callback，避免持鎖跨執行緒
            if (callbackToFire) {
                callbackToFire(data.data(), data.size());
            }
        });
        std::cout << "  ValueChanged registered" << std::endl;

        // 保存 token
        pImpl_->notifyTokens[key] = token;

        // 嘗試寫入 CCCD，失敗時僅警告
        std::cout << "  Writing CCCD..." << std::endl;
        try {
            auto result = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::Notify
            ).get();
            std::cout << "  CCCD done, status=" << (int)result << std::endl;
        } catch (...) {
            std::cout << "  Warning: CCCD write exception, continuing..." << std::endl;
        }

        return true;

    } catch (const hresult_error& ex) {
        std::cout << "subscribeNotify exception: " << to_string(ex.message()) << std::endl;
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
        
        // 1. 找到 characteristic
        auto charIt = pImpl_->characteristics.find(key);
        if (charIt == pImpl_->characteristics.end()) {
            return false;
        }
        
        auto characteristic = charIt->second;
        
        // 2. 移除事件處理器（使用保存的 token）
        auto tokenIt = pImpl_->notifyTokens.find(key);
        if (tokenIt != pImpl_->notifyTokens.end()) {
            try {
                characteristic.ValueChanged(tokenIt->second);
                pImpl_->notifyTokens.erase(tokenIt);
            } catch (...) {
                // 忽略錯誤
            }
        }
        
        // 3. 停用 CCCD（告訴裝置停止發送通知）
        try {
            characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::None
            ).get();
        } catch (...) {
            // 忽略錯誤
        }
        
        // 4. 移除回調
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