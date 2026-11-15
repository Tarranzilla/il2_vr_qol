#pragma once

#include <cstdint>
#include <string>

namespace il2vr {

enum class VJoyAxis : uint32_t {
    X = 0x30,
    Y = 0x31,
    Z = 0x32,
    Rx = 0x33,
    Ry = 0x34,
    Rz = 0x35,
    Sl0 = 0x36,
    Sl1 = 0x37
};

enum class VJoyStatus : int {
    Own = 0,
    Free = 1,
    Busy = 2,
    Missing = 3,
    Unknown = 4
};

class VJoyClient {
public:
    VJoyClient() = default;
    ~VJoyClient();

    VJoyClient(const VJoyClient&) = delete;
    VJoyClient& operator=(const VJoyClient&) = delete;

    bool load();
    void unload();

    bool isLibraryLoaded() const { return module_ != nullptr; }
    bool isDriverEnabled() const;
    int16_t driverVersion() const;
    VJoyStatus deviceStatus(uint32_t deviceId) const;

    bool acquire(uint32_t deviceId);
    void relinquish();
    bool reset();
    bool isAcquired() const { return acquiredDeviceId_ != 0; }
    uint32_t acquiredDeviceId() const { return acquiredDeviceId_; }

    bool axisExists(uint32_t deviceId, VJoyAxis axis) const;
    bool hasRequiredFlightAxes(uint32_t deviceId = 1) const;
    std::string describeEnabledAxes(uint32_t deviceId = 1) const;
    bool configureAllAxesWithAdmin(uint32_t deviceId = 1);

    bool setAxisRaw(VJoyAxis axis, int32_t value);
    bool setAxisNormalized(VJoyAxis axis, float valueMinus1To1);
    bool setThrottleNormalized(VJoyAxis axis, float value0To1);
    bool setButton(uint8_t button1Based, bool pressed);

    const std::string& loadedPath() const { return loadedPath_; }

    static int32_t normalizedToVJoyAxis(float valueMinus1To1);
    static int32_t throttleNormalizedToVJoyAxis(float value0To1);

private:
    bool reacquireCurrentDevice();

    void* module_ = nullptr;
    uint32_t acquiredDeviceId_ = 0;
    std::string loadedPath_;

    struct FnTable;
    FnTable* fn_ = nullptr;
};

} // namespace il2vr
