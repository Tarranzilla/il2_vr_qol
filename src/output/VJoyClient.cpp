#include "output/VJoyClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>

namespace il2vr {

struct VJoyClient::FnTable {
    using VJoyEnabledFn = BOOL(__cdecl*)();
    using GetvJoyVersionFn = SHORT(__cdecl*)();
    using GetVJDStatusFn = int(__cdecl*)(UINT rID);
    using AcquireVJDFn = BOOL(__cdecl*)(UINT rID);
    using RelinquishVJDFn = VOID(__cdecl*)(UINT rID);
    using ResetVJDFn = BOOL(__cdecl*)(UINT rID);
    using GetVJDAxisExistFn = int(__cdecl*)(UINT rID, UINT Axis);
    using SetAxisFn = BOOL(__cdecl*)(LONG Value, UINT rID, UINT Axis);
    using SetBtnFn = BOOL(__cdecl*)(BOOL Value, UINT rID, UCHAR nBtn);

    VJoyEnabledFn vJoyEnabled = nullptr;
    GetvJoyVersionFn GetvJoyVersion = nullptr;
    GetVJDStatusFn GetVJDStatus = nullptr;
    AcquireVJDFn AcquireVJD = nullptr;
    RelinquishVJDFn RelinquishVJD = nullptr;
    ResetVJDFn ResetVJD = nullptr;
    GetVJDAxisExistFn GetVJDAxisExist = nullptr;
    SetAxisFn SetAxis = nullptr;
    SetBtnFn SetBtn = nullptr;
};

VJoyClient::~VJoyClient() {
    unload();
}

bool VJoyClient::load() {
    if (module_ != nullptr) {
        return true;
    }

    const wchar_t* candidates[] = {
        L"vJoyInterface.dll",
        L"C:\\Program Files\\vJoy\\x64\\vJoyInterface.dll"
    };

    HMODULE mod = nullptr;
    for (const wchar_t* path : candidates) {
        mod = LoadLibraryW(path);
        if (mod != nullptr) {
            char utf8[512]{};
            WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, static_cast<int>(sizeof(utf8)), nullptr, nullptr);
            loadedPath_ = utf8;
            break;
        }
    }

    if (mod == nullptr) {
        return false;
    }

    auto* table = new FnTable();
    table->vJoyEnabled = reinterpret_cast<FnTable::VJoyEnabledFn>(GetProcAddress(mod, "vJoyEnabled"));
    table->GetvJoyVersion = reinterpret_cast<FnTable::GetvJoyVersionFn>(GetProcAddress(mod, "GetvJoyVersion"));
    table->GetVJDStatus = reinterpret_cast<FnTable::GetVJDStatusFn>(GetProcAddress(mod, "GetVJDStatus"));
    table->AcquireVJD = reinterpret_cast<FnTable::AcquireVJDFn>(GetProcAddress(mod, "AcquireVJD"));
    table->RelinquishVJD = reinterpret_cast<FnTable::RelinquishVJDFn>(GetProcAddress(mod, "RelinquishVJD"));
    table->ResetVJD = reinterpret_cast<FnTable::ResetVJDFn>(GetProcAddress(mod, "ResetVJD"));
    table->GetVJDAxisExist = reinterpret_cast<FnTable::GetVJDAxisExistFn>(GetProcAddress(mod, "GetVJDAxisExist"));
    table->SetAxis = reinterpret_cast<FnTable::SetAxisFn>(GetProcAddress(mod, "SetAxis"));
    table->SetBtn = reinterpret_cast<FnTable::SetBtnFn>(GetProcAddress(mod, "SetBtn"));

    if (!table->vJoyEnabled || !table->GetVJDStatus || !table->AcquireVJD ||
        !table->RelinquishVJD || !table->ResetVJD || !table->SetAxis || !table->SetBtn) {
        delete table;
        FreeLibrary(mod);
        loadedPath_.clear();
        return false;
    }

    module_ = mod;
    fn_ = table;
    return true;
}

void VJoyClient::unload() {
    relinquish();
    delete fn_;
    fn_ = nullptr;
    if (module_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(module_));
        module_ = nullptr;
    }
    loadedPath_.clear();
}

bool VJoyClient::isDriverEnabled() const {
    if (!fn_ || !fn_->vJoyEnabled) {
        return false;
    }
    return fn_->vJoyEnabled() != FALSE;
}

int16_t VJoyClient::driverVersion() const {
    if (!fn_ || !fn_->GetvJoyVersion) {
        return 0;
    }
    return static_cast<int16_t>(fn_->GetvJoyVersion());
}

VJoyStatus VJoyClient::deviceStatus(uint32_t deviceId) const {
    if (!fn_ || !fn_->GetVJDStatus) {
        return VJoyStatus::Unknown;
    }
    const int st = fn_->GetVJDStatus(deviceId);
    if (st >= 0 && st <= 4) {
        return static_cast<VJoyStatus>(st);
    }
    return VJoyStatus::Unknown;
}

bool VJoyClient::acquire(uint32_t deviceId) {
    if (!fn_ && !load()) {
        return false;
    }
    if (!isDriverEnabled()) {
        return false;
    }
    if (acquiredDeviceId_ == deviceId) {
        return true;
    }
    relinquish();
    if (fn_->AcquireVJD(deviceId) == FALSE) {
        return false;
    }
    acquiredDeviceId_ = deviceId;
    return true;
}

void VJoyClient::relinquish() {
    if (acquiredDeviceId_ != 0 && fn_ && fn_->RelinquishVJD) {
        fn_->RelinquishVJD(acquiredDeviceId_);
    }
    acquiredDeviceId_ = 0;
}

bool VJoyClient::reset() {
    if (acquiredDeviceId_ == 0 || !fn_ || !fn_->ResetVJD) {
        return false;
    }
    return fn_->ResetVJD(acquiredDeviceId_) != FALSE;
}

bool VJoyClient::axisExists(uint32_t deviceId, VJoyAxis axis) const {
    if (!fn_ || !fn_->GetVJDAxisExist) {
        return false;
    }
    // Missing axes return negative error codes (e.g., -10), so check > 0 rather than != 0.
    return fn_->GetVJDAxisExist(deviceId, static_cast<UINT>(axis)) > 0;
}

bool VJoyClient::hasRequiredFlightAxes(uint32_t deviceId) const {
    return axisExists(deviceId, VJoyAxis::X) &&
        axisExists(deviceId, VJoyAxis::Y) &&
        axisExists(deviceId, VJoyAxis::Z) &&
        axisExists(deviceId, VJoyAxis::Rz);
}

std::string VJoyClient::describeEnabledAxes(uint32_t deviceId) const {
    if (!isLibraryLoaded() || !isDriverEnabled()) {
        return "vJoy Offline";
    }
    const auto tag = [&](const char* name, VJoyAxis ax) -> std::string {
        return std::string(name) + (axisExists(deviceId, ax) ? ":OK" : ":FALTA");
    };
    return tag("X", VJoyAxis::X) + " " +
        tag("Y", VJoyAxis::Y) + " " +
        tag("Z", VJoyAxis::Z) + " " +
        tag("Rz", VJoyAxis::Rz);
}

bool VJoyClient::configureAllAxesWithAdmin(uint32_t deviceId) {
    const bool wasAcquired = (acquiredDeviceId_ != 0);
    relinquish();

    const std::wstring params =
        std::to_wstring(deviceId) + L" -f -a x y z rx ry rz sl0 sl1 -b 16";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"C:\\Program Files\\vJoy\\x64\\vJoyConfig.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        if (wasAcquired) {
            acquire(deviceId);
        }
        return false;
    }

    if (sei.hProcess != nullptr) {
        WaitForSingleObject(sei.hProcess, 15000);
        CloseHandle(sei.hProcess);
    }

    Sleep(500);
    if (wasAcquired) {
        acquire(deviceId);
    }
    return hasRequiredFlightAxes(deviceId);
}

bool VJoyClient::reacquireCurrentDevice() {
    if (acquiredDeviceId_ == 0 || !fn_ || !fn_->RelinquishVJD || !fn_->AcquireVJD) {
        return false;
    }
    const uint32_t targetId = acquiredDeviceId_;
    fn_->RelinquishVJD(targetId);
    if (fn_->AcquireVJD(targetId) != FALSE) {
        return true;
    }
    acquiredDeviceId_ = 0;
    return false;
}

bool VJoyClient::setAxisRaw(VJoyAxis axis, int32_t value) {
    if (acquiredDeviceId_ == 0 || !fn_ || !fn_->SetAxis) {
        return false;
    }
    const LONG clamped = static_cast<LONG>(std::clamp(value, 1, 32768));
    if (fn_->SetAxis(clamped, acquiredDeviceId_, static_cast<UINT>(axis)) != FALSE) {
        return true;
    }
    if (deviceStatus(acquiredDeviceId_) != VJoyStatus::Own && reacquireCurrentDevice()) {
        return fn_->SetAxis(clamped, acquiredDeviceId_, static_cast<UINT>(axis)) != FALSE;
    }
    return false;
}

bool VJoyClient::setAxisNormalized(VJoyAxis axis, float valueMinus1To1) {
    return setAxisRaw(axis, normalizedToVJoyAxis(valueMinus1To1));
}

bool VJoyClient::setThrottleNormalized(VJoyAxis axis, float value0To1) {
    return setAxisRaw(axis, throttleNormalizedToVJoyAxis(value0To1));
}

bool VJoyClient::setButton(uint8_t button1Based, bool pressed) {
    if (acquiredDeviceId_ == 0 || !fn_ || !fn_->SetBtn || button1Based == 0) {
        return false;
    }
    const BOOL val = pressed ? TRUE : FALSE;
    if (fn_->SetBtn(val, acquiredDeviceId_, button1Based) != FALSE) {
        return true;
    }
    if (deviceStatus(acquiredDeviceId_) != VJoyStatus::Own && reacquireCurrentDevice()) {
        return fn_->SetBtn(val, acquiredDeviceId_, button1Based) != FALSE;
    }
    return false;
}

int32_t VJoyClient::normalizedToVJoyAxis(float valueMinus1To1) {
    const float c = std::clamp(valueMinus1To1, -1.0f, 1.0f);
    const long val = 16384L + std::lround(c * 16384.0f);
    return static_cast<int32_t>(std::clamp<long>(val, 1L, 32768L));
}

int32_t VJoyClient::throttleNormalizedToVJoyAxis(float value0To1) {
    const float c = std::clamp(value0To1, 0.0f, 1.0f);
    const long val = 1L + std::lround(c * 32767.0f);
    return static_cast<int32_t>(std::clamp<long>(val, 1L, 32768L));
}

} // namespace il2vr
