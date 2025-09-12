#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <string>
#include <memory>

class AudioEndpointController {
public:
    AudioEndpointController() = default;
    ~AudioEndpointController();

    // Inizializza COM per questo controller (MTA semplice)
    bool init();

    // Seleziona il device endpoint per ID IMM (esposto da EnumerateDevicesJson)
    // Esempio di id: "{0.0.0.00000000}.{e4f7...}"
    bool setActiveEndpointById(const std::wstring& deviceId, std::wstring& err);

    // Volume [0.0 .. 1.0]
    bool setVolumeScalar(float vol, std::wstring& err);
    bool getVolumeScalar(float& out, std::wstring& err);
    bool setMute(bool mute, std::wstring& err);

    // Facile da interrogare dal FE
    std::wstring currentDeviceId() const { return m_currentId; }

private:
    // Riferimenti COM
    IMMDeviceEnumerator* m_enum = nullptr;
    IMMDevice* m_device = nullptr;
    IAudioEndpointVolume* m_epvol = nullptr;

    std::wstring m_currentId;

    void releaseDevice_();
};
