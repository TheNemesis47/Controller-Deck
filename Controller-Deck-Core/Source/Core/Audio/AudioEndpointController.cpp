#include "AudioEndpointController.hpp"
#include <atlbase.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")

AudioEndpointController::~AudioEndpointController() {
    releaseDevice_();
    if (m_enum) { m_enum->Release(); m_enum = nullptr; }
    CoUninitialize();
}

bool AudioEndpointController::init() {
    // COM (se già inizializzato altrove in STA, va comunque bene)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&m_enum);
    return SUCCEEDED(hr) && m_enum;
}

void AudioEndpointController::releaseDevice_() {
    if (m_epvol) { m_epvol->Release(); m_epvol = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
    m_currentId.clear();
}

bool AudioEndpointController::setActiveEndpointById(const std::wstring& deviceId, std::wstring& err) {
    if (!m_enum) { err = L"enumerator_not_initialized"; return false; }

    releaseDevice_();

    HRESULT hr = m_enum->GetDevice(deviceId.c_str(), &m_device);
    if (FAILED(hr) || !m_device) { err = L"device_not_found"; return false; }

    hr = m_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&m_epvol);
    if (FAILED(hr) || !m_epvol) { err = L"endpoint_volume_unavailable"; releaseDevice_(); return false; }

    m_currentId = deviceId;
    return true;
}

bool AudioEndpointController::setVolumeScalar(float vol, std::wstring& err) {
    if (!m_epvol) { err = L"no_active_endpoint"; return false; }
    if (vol < 0.f) vol = 0.f; if (vol > 1.f) vol = 1.f;
    HRESULT hr = m_epvol->SetMasterVolumeLevelScalar(vol, nullptr);
    if (FAILED(hr)) { err = L"set_volume_failed"; return false; }
    return true;
}

bool AudioEndpointController::getVolumeScalar(float& out, std::wstring& err) {
    if (!m_epvol) { err = L"no_active_endpoint"; return false; }
    HRESULT hr = m_epvol->GetMasterVolumeLevelScalar(&out);
    if (FAILED(hr)) { err = L"get_volume_failed"; return false; }
    return true;
}

bool AudioEndpointController::setMute(bool mute, std::wstring& err) {
    if (!m_epvol) { err = L"no_active_endpoint"; return false; }
    HRESULT hr = m_epvol->SetMute(mute ? TRUE : FALSE, nullptr);
    if (FAILED(hr)) { err = L"set_mute_failed"; return false; }
    return true;
}
