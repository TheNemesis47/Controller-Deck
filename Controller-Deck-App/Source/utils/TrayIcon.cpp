// utils/TrayIcon.cpp
#include <strsafe.h>
#include "utils/Log.hpp"
#include "TrayIcon.hpp"
#include <objbase.h>
#include <commctrl.h>
#include <string>
#include <stdexcept>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// GUID arbitrario ma stabile per la nostra tray icon
const GUID TrayIcon::TRAY_GUID = { 0x9b3c1b9a, 0x7d6a, 0x4f2f, {0x92,0x74,0x3a,0x61,0xb6,0x9f,0x15,0x3f} };

static TrayIcon* s_instance = nullptr; // per inoltrare messaggi alla istanza

static HICON LoadDefaultIcon() {
    // Fallback: icona di default dell'app (puoi sostituirla con una tua .ico in risorse)
    return LoadIconW(nullptr, IDI_APPLICATION);
}

TrayIcon::TrayIcon(std::wstring tooltipText, std::wstring urlToOpen, QuitCallback onQuit, HICON customIcon)
    : m_tooltip(std::move(tooltipText)),
    m_url(std::move(urlToOpen)),
    m_onQuit(std::move(onQuit)) {
    m_icon = customIcon ? customIcon : LoadDefaultIcon();
}

TrayIcon::~TrayIcon() {
    stop();
}

bool TrayIcon::start() {
    if (m_running.exchange(true)) return false;
    s_instance = this;
    try {
        m_thread = std::thread(&TrayIcon::threadProc, this);
    }
    catch (const std::system_error& e) {
        m_running.store(false);
        s_instance = nullptr;
        LOGF("[Tray] std::system_error on start: {} (code {})", e.what(), (int)e.code().value());
        return false;
    }
    return true;
}

void TrayIcon::stop() {
    if (!m_running.exchange(false)) return;
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }
    if (m_thread.joinable()) m_thread.join();
    s_instance = nullptr;
}

void TrayIcon::threadProc() {
    try{// Finestra nascosta per ricevere i messaggi della tray
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        const wchar_t* kClassName = L"ControllerDeckTrayWnd";

        WNDCLASSW wc{};
        wc.lpfnWndProc = &TrayIcon::WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = kClassName;
        RegisterClassW(&wc);

        // CreateWindow invisibile (message-only window)
        m_hwnd = CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInst, nullptr);
        if (!m_hwnd) {
            m_running.store(false);
            return;
        }

        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        if (!addIcon()) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
            m_running.store(false);
            return;
        }

        // Loop messaggi
        MSG msg{};
        while (m_running.load() && GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        delIcon();

        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }
    catch (const std::exception& e) {
        LOGF("[Tray] thread exception: {}", e.what());
    }
    catch (...) {
        LOGF("[Tray] thread unknown exception");
    }
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    switch (msg) {
    case WM_TRAYICON:
        if (!self) break;
        switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            self->onLeftClick();
            break;
        case WM_RBUTTONUP: {
            POINT pt; GetCursorPos(&pt);
            self->onRightClick(pt);
        } break;
        default: break;
        }
        return 0;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool TrayIcon::addIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON | NIF_GUID;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = m_icon;
    nid.guidItem = TRAY_GUID;

    StringCchCopyW(nid.szTip, _countof(nid.szTip), m_tooltip.c_str());

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        LOGF("[Tray] NIM_ADD failed (GetLastError={})", (int)GetLastError());
        return false;
    }
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    return true;
}

void TrayIcon::delIcon() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_GUID;
    nid.guidItem = TRAY_GUID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void TrayIcon::onLeftClick() {
    openFrontend();
}

void TrayIcon::onRightClick(POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;
    AppendMenuW(hMenu, MF_STRING, 1001, L"Apri interfaccia");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, 1002, L"Esci");

    // Necessario per far funzionare correttamente il menu
    SetForegroundWindow(m_hwnd);
    const UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 1001) openFrontend();
    else if (cmd == 1002) {
        if (m_onQuit) m_onQuit();
    }
}

void TrayIcon::openFrontend() {
    // Apre l’URL nel browser di default
    ShellExecuteW(nullptr, L"open", m_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// Helper se mai ti servisse convertire std::string
std::wstring TrayIcon::widen(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}
