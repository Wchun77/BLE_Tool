#include "BLEManager.h"
#include <stdexcept>

namespace BLE {

BLEManager::BLEManager()
    : backend_(nullptr)
    , initialized_(false)
{
}

BLEManager::~BLEManager() {
    shutdown();
}

bool BLEManager::initialize() {
    if (initialized_) {
        return true;
    }

    try {
        backend_ = createPlatformBackend();
        if (!backend_) {
            handleError("Failed to create platform backend");
            return false;
        }

        if (!backend_->initialize()) {
            handleError("Failed to initialize backend: " + backend_->getLastError());
            return false;
        }

        initialized_ = true;
        return true;

    } catch (const std::exception& e) {
        handleError(std::string("Exception during initialization: ") + e.what());
        return false;
    }
}

void BLEManager::shutdown() {
    if (backend_) {
        backend_->shutdown();
        backend_.reset();
    }
    initialized_ = false;
    scannedDevices_.clear();
}

bool BLEManager::isInitialized() const {
    return initialized_;
}

bool BLEManager::startScan(int timeoutMs) {
    if (!initialized_) {
        handleError("BLE Manager not initialized");
        return false;
    }

    scannedDevices_.clear();

    backend_->setScanCallback([this](const DeviceInfo& device) {
        scannedDevices_.push_back(device);
        if (scanCallback_) {
            scanCallback_(device);
        }
    });

    return backend_->startScan(timeoutMs);
}

void BLEManager::stopScan() {
    if (backend_) {
        backend_->stopScan();
    }
}

void BLEManager::setScanCallback(ScanCallback callback) {
    scanCallback_ = callback;
}

std::vector<DeviceInfo> BLEManager::getScannedDevices() const {
    return scannedDevices_;
}

bool BLEManager::connect(const std::string& deviceAddress) {
    if (!initialized_) {
        handleError("BLE Manager not initialized");
        return false;
    }

    return backend_->connect(deviceAddress);
}

bool BLEManager::disconnect() {
    if (!initialized_) {
        return false;
    }

    return backend_->disconnect();
}

bool BLEManager::isConnected() const {
    if (!initialized_ || !backend_) {
        return false;
    }

    return backend_->isConnected();
}

void BLEManager::setConnectionCallback(ConnectionCallback callback) {
    if (backend_) {
        backend_->setConnectionCallback(callback);
    }
}

// 新增：根據名稱連線
bool BLEManager::connectByName(const std::string& deviceName) {
    if (!initialized_) {
        handleError("BLE Manager not initialized");
        return false;
    }

    // 從掃描結果中找到對應名稱的裝置
    for (const auto& dev : scannedDevices_) {
        if (dev.name == deviceName) {
            return connect(dev.address);
        }
    }

    handleError("Device not found: " + deviceName);
    return false;
}

bool BLEManager::discoverServices() {
    if (!initialized_ || !backend_) {
        handleError("Not initialized or connected");
        return false;
    }

    return backend_->discoverServices();
}

std::vector<std::string> BLEManager::getServices() {
    if (!initialized_ || !backend_) {
        return {};
    }

    return backend_->getServices();
}

std::vector<std::string> BLEManager::getCharacteristics(const std::string& serviceUuid) {
    if (!initialized_ || !backend_) {
        return {};
    }

    return backend_->getCharacteristics(serviceUuid);
}

bool BLEManager::writeData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    const uint8_t* data,
    size_t length,
    bool withResponse)
{
    if (!initialized_ || !backend_) {
        handleError("Not initialized");
        return false;
    }

    return backend_->writeData(serviceUuid, charUuid, data, length, withResponse);
}

bool BLEManager::readData(
    const std::string& serviceUuid,
    const std::string& charUuid,
    std::vector<uint8_t>& outData)
{
    if (!initialized_ || !backend_) {
        handleError("Not initialized");
        return false;
    }

    return backend_->readData(serviceUuid, charUuid, outData);
}

// 新增：寫入字串（便利函數，不包裝格式）
bool BLEManager::writeString(
    const std::string& serviceUuid,
    const std::string& charUuid,
    const std::string& data,
    bool withResponse)
{
    return writeData(
        serviceUuid,
        charUuid,
        reinterpret_cast<const uint8_t*>(data.c_str()),
        data.length(),
        withResponse
    );
}

bool BLEManager::subscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid,
    DataCallback callback)
{
    if (!initialized_ || !backend_) {
        handleError("Not initialized");
        return false;
    }

    return backend_->subscribeNotify(serviceUuid, charUuid, callback);
}

bool BLEManager::unsubscribeNotify(
    const std::string& serviceUuid,
    const std::string& charUuid)
{
    if (!initialized_ || !backend_) {
        return false;
    }

    return backend_->unsubscribeNotify(serviceUuid, charUuid);
}

std::string BLEManager::getLastError() const {
    if (backend_) {
        return backend_->getLastError();
    }
    return "Backend not initialized";
}

void BLEManager::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = callback;
}

void BLEManager::handleError(const std::string& error) {
    if (errorCallback_) {
        errorCallback_(error);
    }
}

} // namespace BLE