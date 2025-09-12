#include "utils/MainApp.hpp"
#include "utils/Log.hpp"
#include <crtdbg.h>
#include <exception>
#include <fmt/format.h>

static void __cdecl OnInvalidParameter(
    const wchar_t* expr, const wchar_t* func, const wchar_t* file,
    unsigned line, uintptr_t) {
    // logga sempre in Output window (Release incluso)
    LOGF("[CRT INVALID PARAM] expr='{}' func='{}' file='{}' line={}",
        expr ? fmt::format("{}", fmt::wstring_view(expr)) : "(null)",
        func ? fmt::format("{}", fmt::wstring_view(func)) : "(null)",
        file ? fmt::format("{}", fmt::wstring_view(file)) : "(null)",
        line);
}

int main() {
    _set_invalid_parameter_handler(OnInvalidParameter);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    try {
        MainApp app{ "Source/config.json" };
        return app.run();
    }
    catch (const fmt::format_error& e) {
        LOGF("[FATAL] fmt::format_error: {}", e.what());
        return 1;
    }
    catch (const std::system_error& e) {
        LOGF("[FATAL] std::system_error: {} (code {})", e.what(), (int)e.code().value());
        return 2;
    }
    catch (const std::exception& e) {
        LOGF("[FATAL] std::exception: {}", e.what());
        return 3;
    }
    catch (...) {
        LOGF("[FATAL] eccezione sconosciuta");
        return 4;
    }
}
