workspace "Controller-Deck"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Controller-Deck"

   -- ✅ Abilita eccezioni in modo portabile
   exceptionhandling "On"         -- (MSVC -> /EHsc)

   -- Opzioni MSVC aggiuntive (ok tenerle)
   filter "system:windows"
      buildoptions { "/Zc:preprocessor", "/Zc:__cplusplus" }
      defines { "_HAS_EXCEPTIONS=1", "FMT_USE_EXCEPTIONS=1" }  -- ✅ forza eccezioni anche per fmt
   filter {}

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

group "Core"
   include "Controller-Deck-Core/Build-Core.lua"
group ""

include "Controller-Deck-App/Build-App.lua"
includedirs { "ThirdParty/cpp-httplib" }
