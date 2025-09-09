// utils/TrayIcon.hpp
#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>

// Classe semplice per creare una tray icon con menu e callback.
// - onQuit: chiamata quando l'utente sceglie "Esci" dal menu.
// - urlToOpen: URL della webapp React da aprire al click.
class TrayIcon {
public:
    using QuitCallback = std::function<void()>;

    TrayIcon(std::wstring tooltipText,
        std::wstring urlToOpen,
        QuitCallback onQuit = nullptr,
        HICON customIcon = nullptr);
    ~TrayIcon();

    // Avvia il thread GUI della tray (message loop WndProc).
    bool start();

    // Arresta e rimuove l’icona in modo pulito.
    void stop();

private:
    // Thread e finestra nascosta per ricevere i messaggi della tray
    std::thread        m_thread;
    std::atomic<bool>  m_running{ false };
    HWND               m_hwnd{ nullptr };
    HICON              m_icon{ nullptr };

    std::wstring       m_tooltip;
    std::wstring       m_url;
    QuitCallback       m_onQuit;

    // Costanti e GUID tray
    static constexpr UINT WM_TRAYICON = WM_APP + 1;
    static const GUID     TRAY_GUID;

    // Funzioni interne
    void threadProc();
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool addIcon();
    void delIcon();
    void onLeftClick();
    void onRightClick(POINT pt);
    void openFrontend();

    static std::wstring widen(const std::string& s);
};
