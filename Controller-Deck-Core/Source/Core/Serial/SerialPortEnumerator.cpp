#include "SerialPortEnumerator.hpp"
#include <iostream>
#include <Windows.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <regstr.h>

#pragma comment(lib, "setupapi.lib")

// GUID for the ports
static const GUID GUID_DEVINTERFACE_COMPORT = { 0x86E0D1E0L, 0x8089, 0x11D0,{ 0x9C,0xE4,0x08,0x00,0x3E,0x30,0x1F,0x73 } };

std::vector<std::string> ListSerialPorts() {
    std::vector<std::string> result;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return result;

    SP_DEVICE_INTERFACE_DATA ifData = {};
    ifData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_COMPORT, i, &ifData); ++i) {
        // ottieni dimensione richiesta
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifData, nullptr, 0, &requiredSize, nullptr);
        std::vector<uint8_t> buffer(requiredSize);
        auto detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        SP_DEVINFO_DATA devInfo = {};
        devInfo.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &ifData, detail, requiredSize, nullptr, &devInfo))
            continue;

        // Leggi "PortName" dal registry (es. "COM3")
        char portName[256] = {};
        if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfo, SPDRP_FRIENDLYNAME, nullptr, reinterpret_cast<PBYTE>(portName), sizeof(portName), nullptr)) {
            // Il FriendlyName spesso contiene "... (COM3)". Estrai COMx
            std::string friendly = portName;
            auto l = friendly.rfind("(COM");
            auto r = friendly.rfind(')');
            if (l != std::string::npos && r != std::string::npos && r > l + 1) {
                std::string com = friendly.substr(l + 1, r - l - 1); // "COM3"
                result.push_back(com);
                continue;
            }
        }

        // fallback: prova a leggere direttamente PortName
        HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &devInfo, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey != INVALID_HANDLE_VALUE) {
            char name[256]; DWORD type = 0, size = sizeof(name);
            if (RegQueryValueExA(hKey, "PortName", nullptr, &type, reinterpret_cast<LPBYTE>(name), &size) == ERROR_SUCCESS && type == REG_SZ) {
                result.emplace_back(name);
            }
            RegCloseKey(hKey);
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return result;
}