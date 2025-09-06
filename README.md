# Controller-Deck-App – Documentazione Tecnica

## 📖 Introduzione
**Controller-Deck-App** è un’applicazione C++ che fornisce un **bridge tra hardware seriale** (controller esterno collegato via COM) e il **sistema audio di Windows**, con un’API REST integrata.  
L’obiettivo è controllare volumi e sessioni audio tramite uno **slider deck** fisico, mantenendo sincronizzazione con i processi attivi e i device audio del sistema.

### Obiettivi principali
- Gestione della **connessione seriale** (apertura, chiusura, selezione porta).
- Controllo di **volumi master e sessioni applicative** tramite mapping slider → sessione.
- Esposizione di **API REST** locali (su `127.0.0.1:8765`) per interrogare e configurare il sistema.
- Supporto alla **persistenza configurazione** (`config.json`).
- Funzionalità di **diagnostica** (health, version, state, ecc.).

---

## 🏗️ Architettura
L’applicazione è organizzata in moduli principali:

- **MainApp**  
  Inizializza e coordina i controller (seriale, audio master, sessioni).  
  Espone le funzioni di configurazione e gestisce il ciclo principale.

- **SerialController**  
  Gestisce la comunicazione con la porta COM (lettura slider e pulsanti).

- **AudioController / AudioSessionController**  
  - `AudioController`: controllo volume master.  
  - `AudioSessionController`: gestione volumi per processo/sessione.

- **MappingExecutor**  
  Si occupa di applicare il mapping slider → volume/mute.

- **ApiServer**  
  Server REST basato su `cpp-httplib`, con supporto opzionale CORS.  
  Gestisce routing e serializzazione JSON (via `nlohmann::json`).

---

## 🌐 API REST

### Base URL
```
http://127.0.0.1:8765
```

### Endpoints disponibili

#### 🔹 System
- **GET `/health`**  
  Verifica che il server sia attivo.  
  ```json
  { "ok": true, "result": { "alive": true } }
  ```

- **GET `/version`**  
  Restituisce versione app e API.  
  ```json
  { "ok": true, "result": { "app": "Controller-Deck", "api": "1.0.0", "build": "dev" } }
  ```

#### 🔹 Config
- **GET `/config`**  
  Restituisce la configurazione attuale (`config.json`).  

- **PUT `/config`**  
  Applica e salva una nuova configurazione.  
  - Richiede un JSON valido.  
  - Risposta OK:  
    ```json
    { "ok": true, "result": { "applied": true } }
    ```

- **PUT `/config/validate`**  
  Valida un JSON di configurazione **senza applicarlo**.  

#### 🔹 Stato Controller
- **GET `/state`**  
  Stato attuale degli slider e pulsanti.  
  ```json
  {
    "ok": true,
    "result": {
      "sliders": [12, 0, 55, 80, 100],
      "buttons": [0, 1, 0, 0, 1]
    }
  }
  ```

#### 🔹 Serial
- **GET `/serial/ports`**  
  Elenca le porte seriali disponibili.  
  ```json
  { "ok": true, "result": { "ports": ["COM3", "COM5"] } }
  ```

- **POST `/serial/select`**  
  Seleziona e apre una porta seriale.  
  ```json
  { "ok": true, "result": { "selected": "COM3", "baud": 115200 } }
  ```

- **POST `/serial/close`**  
  Chiude la porta seriale attiva.  
  - Successo:  
    ```json
    { "ok": true, "result": { "closed": true, "message": "Connessione seriale chiusa; in attesa di nuova selezione" } }
    ```
  - Se già chiusa:  
    ```json
    { "ok": false, "error": "not_connected" }
    ```

#### 🔹 Audio
- **GET `/audio/devices`**  
  Elenco dei device audio attivi.  
  ```json
  {
    "ok": true,
    "result": {
      "render": [{ "id": "...", "name": "Speakers" }],
      "capture": [{ "id": "...", "name": "Microfono" }],
      "default_render_id": "...",
      "default_capture_id": "..."
    }
  }
  ```

- **GET `/audio/processes`**  
  Elenco processi con sessioni audio.  
  ```json
  {
    "ok": true,
    "result": {
      "processes": [
        {
          "exe": "chrome.exe",
          "pids": [1234, 5678],
          "volume": 0.85,
          "mute": false,
          "state": "active",
          "likely_fullscreen": false
        }
      ]
    }
  }
  ```

---

## ✅ Conclusione
Questa documentazione descrive le **funzioni core** e le **API REST** dell’applicazione.  
L’architettura modulare permette di:  
- Estendere facilmente le API.  
- Integrare altre piattaforme (es. macOS/Linux) sostituendo il backend audio/seriale.  
- Usare il controller hardware come **bridge universale** per la gestione dell’audio.  


## License
- UNLICENSE for this repository (see `UNLICENSE.txt` for more details)
- Premake is licensed under BSD 3-Clause (see included LICENSE.txt file for more details)




## Immettere Librerie
- clonare vcpkg
- aprire powershell
- nella cartella vcpkg eseguire .\bootstrap-vcpkg.bat
- nella cartella vcpkg eseguire .\vcpkg integrate install
- nella cartella vcpkg eseguire .\vcpkg search <nome libreria>
- nella cartella vcpkg eseguire .\vcpkg install <nome libreria>
- nella cartella vcpkg eseguire .\vcpkg list per vedere le librerie installate
- nella cartella vcpkg eseguire .\vcpkg remove <nome libreria> per rimuovere una libreria
- nella cartella vcpkg eseguire .\vcpkg update per aggiornare le librerie
- nella cartella vcpkg eseguire .\vcpkg upgrade per aggiornare le librerie installate
- nella cartella vcpkg eseguire .\vcpkg export <nome libreria> --zip per esportare una libreria in un file zip
- nella cartella vcpkg eseguire .\vcpkg integrate project per integrare vcpkg con il progetto corrente
- in visual studio andare in strumenti -> gestione pacchetti nuget -> consola di gestione pacchetti
- nella consola di gestione pacchetti eseguire il comando che vcpkg ha stampato dopo l'integrazione del progetto