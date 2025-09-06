#include "utils/AudioDiscovery.hpp"
#include "utils/ProcessUtils.hpp"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>

using nlohmann::json;

static std::string WideToUtf8(LPCWSTR ws) {
    if (!ws) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(len ? len - 1 : 0, '\0');
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), len, nullptr, nullptr);
    return s;
}

namespace AudioDiscovery {

nlohmann::json EnumerateDevicesJson() {
    json out = { {"render", json::array()}, {"capture", json::array()},
                 {"default_render_id", nullptr}, {"default_capture_id", nullptr} };

    bool needUninit = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) needUninit = true;
    else if (hr != RPC_E_CHANGED_MODE) return json{{"error","coinitialize_failed"}};

    IMMDeviceEnumerator* enumr = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumr)) || !enumr) {
        if (needUninit) CoUninitialize();
        return json{{"error","enumerator_failed"}};
    }

    auto getName = [](IMMDevice* dev)->std::string {
        IPropertyStore* store = nullptr;
        if (FAILED(dev->OpenPropertyStore(STGM_READ, &store)) || !store) return {};
        PROPVARIANT v; PropVariantInit(&v);
        std::string name;
        if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR) {
            name = WideToUtf8(v.pwszVal);
        }
        PropVariantClear(&v);
        store->Release();
        return name;
    };

    auto list = [&](EDataFlow flow, const char* key){
        IMMDeviceCollection* col = nullptr;
        if (FAILED(enumr->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &col)) || !col) return;
        UINT n = 0; col->GetCount(&n);
        for (UINT i=0;i<n;++i) {
            IMMDevice* d = nullptr; if (FAILED(col->Item(i, &d)) || !d) continue;
            LPWSTR wid = nullptr; if (FAILED(d->GetId(&wid)) || !wid) { d->Release(); continue; }
            std::string id = WideToUtf8(wid);
            CoTaskMemFree(wid);
            out[key].push_back(json{{"id", id}, {"name", getName(d)}});
            d->Release();
        }
        col->Release();

        IMMDevice* def = nullptr;
        if (SUCCEEDED(enumr->GetDefaultAudioEndpoint(flow, eMultimedia, &def)) && def) {
            LPWSTR wid = nullptr; if (SUCCEEDED(def->GetId(&wid)) && wid) {
                std::string id = WideToUtf8(wid);
                if (flow == eRender) out["default_render_id"] = id;
                else                 out["default_capture_id"] = id;
                CoTaskMemFree(wid);
            }
            def->Release();
        }
    };

    list(eRender,  "render");
    list(eCapture, "capture");

    enumr->Release();
    if (needUninit) CoUninitialize();
    return out;
}

nlohmann::json EnumerateProcessesJson(std::function<bool(DWORD)> isFullscreen) {
    using nlohmann::json;
    json out = { {"processes", json::array()} };

    bool needUninit = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) needUninit = true;
    else if (hr != RPC_E_CHANGED_MODE) return json{{"error","coinitialize_failed"}};

    IMMDeviceEnumerator* enumr = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), (void**)&enumr)) || !enumr) {
        if (needUninit) CoUninitialize();
        return json{{"error","enumerator_failed"}};
    }

    IMMDevice* dev = nullptr;
    if (FAILED(enumr->GetDefaultAudioEndpoint(eRender, eMultimedia, &dev)) || !dev) {
        enumr->Release(); if (needUninit) CoUninitialize();
        return json{{"error","default_endpoint_failed"}};
    }

    IAudioSessionManager2* mgr2 = nullptr;
    if (FAILED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&mgr2)) || !mgr2) {
        dev->Release(); enumr->Release(); if (needUninit) CoUninitialize();
        return json{{"error","session_manager_failed"}};
    }

    IAudioSessionEnumerator* sesEnum = nullptr;
    if (FAILED(mgr2->GetSessionEnumerator(&sesEnum)) || !sesEnum) {
        mgr2->Release(); dev->Release(); enumr->Release(); if (needUninit) CoUninitialize();
        return json{{"error","session_enum_failed"}};
    }

    int count = 0; sesEnum->GetCount(&count);
    std::map<std::string, json> byExe;

    for (int i=0;i<count;++i) {
        IAudioSessionControl* ctrl = nullptr;
        if (FAILED(sesEnum->GetSession(i, &ctrl)) || !ctrl) continue;

        AudioSessionState st; ctrl->GetState(&st);

        IAudioSessionControl2* ctrl2 = nullptr;
        if (FAILED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2)) || !ctrl2) {
            ctrl->Release(); continue;
        }

        ISimpleAudioVolume* vol = nullptr;
        if (FAILED(ctrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&vol)) || !vol) {
            ctrl2->Release(); ctrl->Release(); continue;
        }

        DWORD pid = 0; ctrl2->GetProcessId(&pid);
        std::string exeLower;
        if (!ProcUtils::PidToExeLower(pid, exeLower)) exeLower = "system_sounds";

        float v = 0.f; (void)vol->GetMasterVolume(&v);
        BOOL m = FALSE; (void)vol->GetMute(&m);
        bool fullscreen = isFullscreen ? isFullscreen(pid) : false;

        auto& e = byExe[exeLower];
        if (e.is_null()) {
            e = json{
                {"exe", exeLower},
                {"pids", json::array({ pid })},
                {"volume", v},
                {"mute", (bool)m},
                {"state", st == AudioSessionStateActive ? "active" :
                          st == AudioSessionStateInactive ? "inactive" : "expired"},
                {"likely_fullscreen", fullscreen}
            };
        } else {
            e["pids"].push_back(pid);
            e["volume"] = v;
            e["mute"]   = (bool)m;
            if (std::string(e["state"]) != "active" &&
                st == AudioSessionStateActive) e["state"] = "active";
            e["likely_fullscreen"] = (bool)e["likely_fullscreen"] || fullscreen;
        }

        vol->Release();
        ctrl2->Release();
        ctrl->Release();
    }

    for (auto& kv : byExe) out["processes"].push_back(kv.second);

    sesEnum->Release();
    mgr2->Release();
    dev->Release();
    enumr->Release();
    if (needUninit) CoUninitialize();

    std::sort(out["processes"].begin(), out["processes"].end(),
        [](const json& a, const json& b){
            int pa = (std::string(a["state"]) == "active") ? 0 : 1;
            int pb = (std::string(b["state"]) == "active") ? 0 : 1;
            if (pa != pb) return pa < pb;
            return std::string(a["exe"]) < std::string(b["exe"]);
        });

    return out;
}

} // namespace AudioDiscovery
