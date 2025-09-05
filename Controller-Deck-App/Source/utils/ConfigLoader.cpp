#include "utils/ConfigLoader.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include "utils/Utils.hpp"

using nlohmann::json;

// Mappa "media:*" in una chord equivalente
static bool MediaNameToChord(const std::string& nameLower, HotkeyChord& out) {
    std::string n = nameLower;
    // Riutilizziamo ParseHotkey con i token media
    if (n == "play_pause" || n == "playpause") { auto c = ParseHotkey("MEDIA_PLAY_PAUSE"); if (c) { out = *c; return true; } }
    if (n == "next") { auto c = ParseHotkey("MEDIA_NEXT");       if (c) { out = *c; return true; } }
    if (n == "prev" || n == "previous") { auto c = ParseHotkey("MEDIA_PREV");       if (c) { out = *c; return true; } }
    if (n == "stop") { auto c = ParseHotkey("MEDIA_STOP");       if (c) { out = *c; return true; } }
    if (n == "volume_up") { auto c = ParseHotkey("VOLUME_UP");        if (c) { out = *c; return true; } }
    if (n == "volume_down") { auto c = ParseHotkey("VOLUME_DOWN");      if (c) { out = *c; return true; } }
    if (n == "volume_mute") { auto c = ParseHotkey("VOLUME_MUTE");      if (c) { out = *c; return true; } }
    return false;
}

static bool parseSliderValue(AppConfig& cfg, std::string& outErr, size_t i, const json& v) {
    SliderTarget tgt;
    if (v.is_string()) {
        std::string s = toLower(v.get<std::string>());
        if (s == "master_volume") { tgt.isMaster = true; cfg.sliderMap[i] = tgt; }
        else { tgt.isMaster = false; tgt.exes.push_back(std::move(s)); cfg.sliderMap[i] = tgt; }
        return true;
    }
    else if (v.is_array()) {
        tgt.isMaster = false;
        for (auto& e : v) {
            if (!e.is_string()) { outErr = "sliders[" + std::to_string(i) + "] contiene elementi non stringa."; return false; }
            tgt.exes.push_back(toLower(e.get<std::string>()));
        }
        if (tgt.exes.empty()) { outErr = "sliders[" + std::to_string(i) + "] array vuoto."; return false; }
        cfg.sliderMap[i] = tgt;
        return true;
    }
    else if (v.is_null()) {
        return true; // non mappato
    }
    outErr = "sliders[" + std::to_string(i) + "] deve essere stringa, array o null.";
    return false;
}

static bool pushButtonAction(AppConfig& cfg, std::string& outErr, size_t i, const std::string& sRaw) {
    std::string s = toLower(sRaw);

    // toggle_mute (master o app)
    if (s == "toggle_mute") {
        cfg.buttonActions[i].push_back(ButtonAction{ BtnActKind::ToggleMuteMaster });
        return true;
    }
    if (s.rfind("toggle_mute:", 0) == 0) {
        ButtonAction a; a.kind = BtnActKind::ToggleMuteApp; a.payload = s.substr(strlen("toggle_mute:"));
        if (a.payload.empty()) { outErr = "toggle_mute:<exe> richiede un processo."; return false; }
        cfg.buttonActions[i].push_back(std::move(a));
        return true;
    }

    // hotkey:* o key:* (key è sinonimo)
    if (s.rfind("hotkey:", 0) == 0 || s.rfind("key:", 0) == 0) {
        const char* pfx = s.rfind("key:", 0) == 0 ? "key:" : "hotkey:";
        std::string spec = s.substr(strlen(pfx));
        if (auto chord = ParseHotkey(spec)) {
            ButtonAction a; a.kind = BtnActKind::Hotkey; a.chord = *chord;
            cfg.buttonActions[i].push_back(std::move(a));
            return true;
        }
        outErr = "hotkey non valida: '" + sRaw + "'";
        return false;
    }

    // media:*
    if (s.rfind("media:", 0) == 0) {
        std::string name = s.substr(strlen("media:"));
        HotkeyChord chord{};
        if (!MediaNameToChord(name, chord)) { outErr = "media sconosciuta: '" + sRaw + "'"; return false; }
        ButtonAction a; a.kind = BtnActKind::Media; a.chord = chord; a.payload = name;
        cfg.buttonActions[i].push_back(std::move(a));
        return true;
    }

    // text:*  (testo UTF-8 libero, spazi ammessi)
    if (s.rfind("text:", 0) == 0) {
        std::string txt = sRaw.substr(strlen("text:")); // usa l'originale per preservare maiuscole e simboli
        ButtonAction a; a.kind = BtnActKind::Text; a.payload = txt;
        cfg.buttonActions[i].push_back(std::move(a));
        return true;
    }

    // delay:NNN (millisecondi)
    if (s.rfind("delay:", 0) == 0) {
        std::string msStr = s.substr(strlen("delay:"));
        try {
            int v = std::stoi(msStr);
            if (v < 0) { outErr = "delay deve essere >= 0"; return false; }
            ButtonAction a; a.kind = BtnActKind::Delay; a.delayMs = (unsigned)v;
            cfg.buttonActions[i].push_back(std::move(a));
            return true;
        }
        catch (...) {
            outErr = "delay non valido: '" + sRaw + "'";
            return false;
        }
    }

    outErr = "azione sconosciuta: '" + sRaw + "'";
    return false;
}

static bool parseButtonValue(AppConfig& cfg, std::string& outErr, size_t i, const json& b) {
    if (b.is_null()) return true;
    if (b.is_string()) {
        return pushButtonAction(cfg, outErr, i, b.get<std::string>());
    }
    if (b.is_array()) {
        if (b.empty()) { outErr = "buttons[" + std::to_string(i) + "] array vuoto."; return false; }
        for (auto& x : b) {
            if (!x.is_string()) { outErr = "buttons[" + std::to_string(i) + "] contiene elementi non stringa."; return false; }
            if (!pushButtonAction(cfg, outErr, i, x.get<std::string>())) return false;
        }
        return true;
    }
    outErr = "buttons[" + std::to_string(i) + "] deve essere stringa, array o null.";
    return false;
}

bool LoadConfigStrict(AppConfig& cfg, std::string& outErr, const std::string& configPath) {
    cfg = {};
    for (auto& s : cfg.sliderMap) s.reset();
    for (auto& v : cfg.buttonActions) v.clear();

    try {
        std::ifstream f(configPath);
        if (!f) { outErr = "Impossibile aprire il file: " + configPath; return false; }

        json j; f >> j; // può lanciare

        // serial
        if (!j.contains("serial") || !j["serial"].is_object()) { outErr = "Chiave 'serial' mancante o non oggetto."; return false; }
        auto s = j["serial"];
        if (!s.contains("port") || !s["port"].is_string()) { outErr = "Chiave 'serial.port' mancante o non stringa."; return false; }
        if (!s.contains("baud") || !s["baud"].is_number_unsigned()) { outErr = "Chiave 'serial.baud' mancante o non intero positivo."; return false; }
        cfg.port = s["port"].get<std::string>();
        cfg.baud = s["baud"].get<unsigned>();

        // mapping
        if (!j.contains("mapping") || !j["mapping"].is_object()) { outErr = "Chiave 'mapping' mancante o non oggetto."; return false; }
        auto m = j["mapping"];

        if (!m.contains("sliders") || !m["sliders"].is_array()) { outErr = "Chiave 'mapping.sliders' mancante o non array."; return false; }
        if (m["sliders"].size() > 5) { outErr = "'mapping.sliders' può contenere al massimo 5 elementi."; return false; }
        for (size_t i = 0; i < m["sliders"].size(); ++i) if (!parseSliderValue(cfg, outErr, i, m["sliders"][i])) return false;

        if (!m.contains("buttons") || !m["buttons"].is_array()) { outErr = "Chiave 'mapping.buttons' mancante o non array."; return false; }
        if (m["buttons"].size() > 5) { outErr = "'mapping.buttons' può contenere al massimo 5 elementi."; return false; }
        for (size_t i = 0; i < m["buttons"].size(); ++i) if (!parseButtonValue(cfg, outErr, i, m["buttons"][i])) return false;

        // almeno un mapping presente
        bool any = false, anyBtn = false;
        for (auto& sopt : cfg.sliderMap) if (sopt.has_value()) { any = true; break; }
        for (int i = 0; i < 5; ++i) if (!cfg.buttonActions[i].empty()) { anyBtn = true; break; }
        if (!any && !anyBtn) { outErr = "Nessun mapping configurato (sliders e buttons sono tutti null)."; return false; }

        return true;
    }
    catch (const std::exception& ex) {
        outErr = std::string("Errore di parsing JSON: ") + ex.what() + ". Ricorda: il JSON standard non supporta i commenti.";
        return false;
    }
    catch (...) {
        outErr = "Errore sconosciuto durante la lettura del config.";
        return false;
    }
}
