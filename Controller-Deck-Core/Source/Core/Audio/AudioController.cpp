#include "Core/Audio/AudioController.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include <objbase.h> // CoInitializeEx
#include <cmath>

static void safeRelease(IUnknown*& p) { if (p) { p->Release(); p = nullptr; } }

bool AudioController::init() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) m_comInit = true;
    else if (hr == RPC_E_CHANGED_MODE) {
        // già inizializzato in STA: proviamo comunque a proseguire
    }
    else {
        return false;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) return false;

    IMMDevice* endpoint = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &endpoint);
    if (FAILED(hr)) { enumerator->Release(); return false; }

    IAudioEndpointVolume* epVol = nullptr;
    hr = endpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&epVol);
    if (FAILED(hr)) { endpoint->Release(); enumerator->Release(); return false; }

    m_enumerator = enumerator;
    m_endpoint = endpoint;
    m_epVolume = epVol;
    return true;
}

void AudioController::shutdown() {
    if (m_epVolume) { ((IAudioEndpointVolume*)m_epVolume)->Release(); m_epVolume = nullptr; }
    if (m_endpoint) { ((IMMDevice*)m_endpoint)->Release(); m_endpoint = nullptr; }
    if (m_enumerator) { ((IMMDeviceEnumerator*)m_enumerator)->Release(); m_enumerator = nullptr; }
    if (m_comInit) { CoUninitialize(); m_comInit = false; }
}

bool AudioController::setMasterVolume(float v01) {
    if (!m_epVolume) return false;
    if (v01 < 0.f) v01 = 0.f; if (v01 > 1.f) v01 = 1.f;
    return SUCCEEDED(((IAudioEndpointVolume*)m_epVolume)->SetMasterVolumeLevelScalar(v01, nullptr));
}

bool AudioController::getMasterVolume(float& out) {
    if (!m_epVolume) return false;
    return SUCCEEDED(((IAudioEndpointVolume*)m_epVolume)->GetMasterVolumeLevelScalar(&out));
}

bool AudioController::setMasterMute(bool mute) {
    if (!m_epVolume) return false;
    return SUCCEEDED(((IAudioEndpointVolume*)m_epVolume)->SetMute(mute, nullptr));
}

bool AudioController::toggleMasterMute() {
    if (!m_epVolume) return false;
    BOOL isMuted = FALSE;
    if (FAILED(((IAudioEndpointVolume*)m_epVolume)->GetMute(&isMuted))) return false;
    return SUCCEEDED(((IAudioEndpointVolume*)m_epVolume)->SetMute(!isMuted, nullptr));
}
