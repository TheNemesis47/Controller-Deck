#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cctype>

// Controller per il volume per-app tramite Audio Sessions (WASAPI).
// Identifichiamo le sessioni per nome eseguibile (es. "spotify.exe").
// Nota: i processi possono avere più sessioni (tab di browser, ecc.): applichiamo a tutte.

class AudioSessionController {
public:
    bool init();               // COM + endpoint + IAudioSessionManager2
    void shutdown();

    // Volume [0..1] applicato a TUTTE le sessioni del processo indicato
    bool setAppVolume(const std::string& processExeLower, float v01);

    // Legge il volume dalla prima sessione trovata (se ce n’è almeno una)
    bool getAppVolume(const std::string& processExeLower, float& out01);

    bool setAppMute(const std::string& processExeLower, bool mute);
    bool toggleAppMute(const std::string& processExeLower);

    // Utility: normalizza "C:\\path\\Spotify.exe" -> "spotify.exe"
    static std::string basenameLower(const std::string& fullPath);

private:
    bool withSessions(const std::function<bool(void* /*IAudioSessionControl2*/, void* /*ISimpleAudioVolume*/, unsigned /*pid*/, const std::string& /*exeLower*/)>& fn);
    bool matchProcessName(unsigned pid, const std::string& wantedExeLower, std::string& outExeLower);

    void* m_enumerator = nullptr;       // IMMDeviceEnumerator*
    void* m_device = nullptr;           // IMMDevice*
    void* m_sessionMgr2 = nullptr;      // IAudioSessionManager2*
    bool  m_comInit = false;
};
