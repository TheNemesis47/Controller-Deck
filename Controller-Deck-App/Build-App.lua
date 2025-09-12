project "Controller-Deck-App"
    kind "ConsoleApp"              -- Debug: console per i log
    language "C++"
    cppdialect "C++20"
    characterset "Unicode"
    staticruntime "off"

    -- ✅ Eccezioni ON a livello progetto (ridondante ma sicuro)
    exceptionhandling "On"
    defines { "_HAS_EXCEPTIONS=1", "FMT_USE_EXCEPTIONS=1" }

    -- OUTPUT (lascia il tuo OutputDir se già definito altrove)
    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir    ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    files {
        "Source/**.h",
        "Source/**.hpp",
        "Source/**.cpp",
        -- risorse per icona tray (facoltativo ma consigliato)
        "res/**.rc",
        "Assets/**.ico"
    }

    includedirs {
        "Source",
        "../Controller-Deck-Core/Source"   -- include Core
    }

    links {
        "Controller-Deck-Core"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "_WIN32_WINNT=0x0A00", "WIN32_LEAN_AND_MEAN", "NOMINMAX", "ASIO_STANDALONE" }
        buildoptions { "/utf-8" }

        -- 🔗 Librerie Win32 necessarie per la tray (+ le tue già presenti)
        links {
            "User32", "Gdi32", "Comctl32", "Shell32",  -- tray/menu/icone
            "setupapi", "ws2_32", "mswsock", "advapi32", "ole32", "uuid", "mmdevapi"
        }
    filter {}

    -- DEBUG: tieni la console (comodo per printf/log)
    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"
        kind "ConsoleApp"
    filter {}

    -- RELEASE: niente console ma resta main()
    filter "configurations:Release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "Full"
        symbols "On"
        kind "WindowedApp"                  -- -> /SUBSYSTEM:WINDOWS
        linkoptions { "/ENTRY:mainCRTStartup" } -- usa main() senza WinMain, nessuna console
        flags { "LinkTimeOptimization" }
    filter {}

    -- DIST (se vuoi uguale a Release)
    filter "configurations:Dist"
        defines { "DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"
        kind "WindowedApp"
        linkoptions { "/ENTRY:mainCRTStartup" }
    filter {}
