#pragma once
#include <cstdint>

// Controller per volume di sistema (endpoint predefinito)
// NOTE: usa COM/WASAPI; inizializza alla start e rilascia alla stop.
class AudioController {
public:
    bool init();                       // inizializza COM + endpoint volume
    void shutdown();                   // rilascia tutto

    bool setMasterVolume(float v01);   // 0.0..1.0
    bool getMasterVolume(float& out);  // legge 0.0..1.0
    bool setMasterMute(bool mute);
    bool toggleMasterMute();

private:
    void* m_enumerator = nullptr;      // IMMDeviceEnumerator*
    void* m_endpoint = nullptr;      // IMMDevice*
    void* m_epVolume = nullptr;      // IAudioEndpointVolume*
    bool  m_comInit = false;
};
