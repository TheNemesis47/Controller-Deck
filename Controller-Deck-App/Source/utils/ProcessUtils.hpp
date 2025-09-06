#pragma once
#include <string>
#include <Windows.h>

namespace ProcUtils {
    // Restituisce basename dell'eseguibile in minuscolo per un PID (true se OK)
    bool PidToExeLower(DWORD pid, std::string& exeLower);

    // Utile se ti serve altrove: "C:\\path\\Foo.EXE" -> "foo.exe"
    std::string BasenameLower(const std::string& fullPath);
}
