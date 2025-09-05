#include "Core/Audio/AudioSessionController.hpp"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>  // IAudioSessionManager2, IAudioSessionControl2, ISimpleAudioVolume
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string AudioSessionController::basenameLower(const std::string& fullPath) {
    std::string s = toLower(fullPath);
    size_t p1 = s.find_last_of("\\/");
    if (p1 != std::string::npos) s = s.substr(p1 + 1);
    return s;
}

bool AudioSessionController::init() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) m_comInit = true;
    else if (hr != RPC_E_CHANGED_MODE) return false; // se STA, proviamo a proseguire comunque

    IMMDeviceEnumerator* enumr = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumr);
    if (FAILED(hr)) return false;

    IMMDevice* dev = nullptr;
    hr = enumr->GetDefaultAudioEndpoint(eRender, eMultimedia, &dev);
    if (FAILED(hr)) { enumr->Release(); return false; }

    IAudioSessionManager2* mgr2 = nullptr;
    hr = dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&mgr2);
    if (FAILED(hr)) { dev->Release(); enumr->Release(); return false; }

    m_enumerator = enumr;
    m_device = dev;
    m_sessionMgr2 = mgr2;
    return true;
}

void AudioSessionController::shutdown() {
    if (m_sessionMgr2) { ((IAudioSessionManager2*)m_sessionMgr2)->Release(); m_sessionMgr2 = nullptr; }
    if (m_device) { ((IMMDevice*)m_device)->Release(); m_device = nullptr; }
    if (m_enumerator) { ((IMMDeviceEnumerator*)m_enumerator)->Release(); m_enumerator = nullptr; }
    if (m_comInit) { CoUninitialize(); m_comInit = false; }
}

// Ottiene exeLower da PID (usa QueryFullProcessImageNameA, no psapi)
static bool pidToExeLower(DWORD pid, std::string& exeLower) {
    if (pid == 0) return false; // "System Sounds"
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    char buf[1024]; DWORD sz = (DWORD)sizeof(buf);
    bool ok = false;
    if (QueryFullProcessImageNameA(h, 0, buf, &sz)) {
        exeLower = AudioSessionController::basenameLower(std::string(buf, sz));
        ok = true;
    }
    CloseHandle(h);
    return ok;
}

bool AudioSessionController::matchProcessName(unsigned pid, const std::string& wantedExeLower, std::string& outExeLower) {
    std::string exeLower;
    if (!pidToExeLower((DWORD)pid, exeLower)) return false;
    outExeLower = exeLower;
    return exeLower == wantedExeLower;
}

bool AudioSessionController::withSessions(
    const std::function<bool(void*, void*, unsigned, const std::string&)>& fn) {
    if (!m_sessionMgr2) return false;

    IAudioSessionEnumerator* enumr = nullptr;
    HRESULT hr = ((IAudioSessionManager2*)m_sessionMgr2)->GetSessionEnumerator(&enumr);
    if (FAILED(hr) || !enumr) return false;

    int count = 0;
    enumr->GetCount(&count);
    bool any = false;

    for (int i = 0; i < count; ++i) {
        IAudioSessionControl* ctrl = nullptr;
        if (FAILED(enumr->GetSession(i, &ctrl)) || !ctrl) continue;

        IAudioSessionControl2* ctrl2 = nullptr;
        if (FAILED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2)) || !ctrl2) {
            ctrl->Release(); continue;
        }

        ISimpleAudioVolume* vol = nullptr;
        if (FAILED(ctrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol)) || !vol) {
            ctrl2->Release(); ctrl->Release(); continue;
        }

        DWORD pid = 0;
        if (SUCCEEDED(ctrl2->GetProcessId(&pid))) {
            std::string exeLower;
            // Passiamo i pointer come void* (non **)
            any |= fn((void*)ctrl2, (void*)vol, (unsigned)pid, exeLower);
        }

        vol->Release();
        ctrl2->Release();
        ctrl->Release();
    }

    enumr->Release();
    return any;
}


bool AudioSessionController::setAppVolume(const std::string& processExeLower, float v01) {
    if (!m_sessionMgr2) return false;
    std::string wanted = toLower(processExeLower);
    if (v01 < 0.f) v01 = 0.f; if (v01 > 1.f) v01 = 1.f;

    bool applied = false;
    withSessions([&](void* pCtrl2, void* pVol, unsigned pid, const std::string&) {
        std::string exeLower;
        if (!pidToExeLower(pid, exeLower)) return false;
        if (exeLower != wanted) return false;
        auto* vol = (ISimpleAudioVolume*)pVol;
        if (SUCCEEDED(vol->SetMasterVolume(v01, nullptr))) applied = true;
        return applied;
        });
    return applied;
}

bool AudioSessionController::getAppVolume(const std::string& processExeLower, float& out01) {
    if (!m_sessionMgr2) return false;
    std::string wanted = toLower(processExeLower);
    bool got = false;

    withSessions([&](void* pCtrl2, void* pVol, unsigned pid, const std::string&) {
        std::string exeLower;
        if (!pidToExeLower(pid, exeLower)) return false;
        if (exeLower != wanted) return false;
        float v = 0.f;
        if (SUCCEEDED(((ISimpleAudioVolume*)pVol)->GetMasterVolume(&v))) {
            out01 = v; got = true;
        }
        return got;
        });
    return got;
}

bool AudioSessionController::setAppMute(const std::string& processExeLower, bool mute) {
    if (!m_sessionMgr2) return false;
    std::string wanted = toLower(processExeLower);
    bool applied = false;

    withSessions([&](void* pCtrl2, void* pVol, unsigned pid, const std::string&) {
        std::string exeLower;
        if (!pidToExeLower(pid, exeLower)) return false;
        if (exeLower != wanted) return false;
        if (SUCCEEDED(((ISimpleAudioVolume*)pVol)->SetMute(mute, nullptr))) applied = true;
        return applied;
        });
    return applied;
}

bool AudioSessionController::toggleAppMute(const std::string& processExeLower) {
    if (!m_sessionMgr2) return false;
    std::string wanted = toLower(processExeLower);
    bool toggled = false;

    withSessions([&](void* pCtrl2, void* pVol, unsigned pid, const std::string&) {
        std::string exeLower;
        if (!pidToExeLower(pid, exeLower)) return false;
        if (exeLower != wanted) return false;
        BOOL isMuted = FALSE;
        auto* vol = (ISimpleAudioVolume*)pVol;
        if (FAILED(vol->GetMute(&isMuted))) return false;
        if (SUCCEEDED(vol->SetMute(!isMuted, nullptr))) toggled = true;
        return toggled;
        });
    return toggled;
}
