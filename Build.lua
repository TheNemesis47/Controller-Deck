workspace "Controller-Deck"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Controller-Deck"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

group "Core"
	include "Controller-Deck-Core/Build-Core.lua"
group ""

include "Controller-Deck-App/Build-App.lua"
includedirs {"ThirdParty/cpp-httplib"}