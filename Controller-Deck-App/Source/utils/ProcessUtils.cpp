#include "Utils/ProcessUtils.hpp"
#include <algorithm>
#include <cctype>

namespace {
    static inline std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }
}

std::string ProcUtils::BasenameLower(const std::string& fullPath) {
    std::string s = toLower(fullPath);
    size_t p = s.find_last_of("\\/");
    if (p != std::string::npos) s = s.substr(p + 1);
    return s;
}

bool ProcUtils::PidToExeLower(DWORD pid, std::string& exeLower) {
    if (pid == 0) return false; // "System Sounds"
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;

    char buf[MAX_PATH]; DWORD sz = (DWORD)sizeof(buf);
    bool ok = false;
    if (QueryFullProcessImageNameA(h, 0, buf, &sz)) {
        exeLower = BasenameLower(std::string(buf, sz));
        ok = true;
    }
    CloseHandle(h);
    return ok;
}
