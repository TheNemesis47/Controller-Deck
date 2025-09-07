project "Controller-Deck-App"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   characterset "Unicode"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files { 
       "Source/**.h", 
       "Source/**.cpp",
       "Source/**.hpp"
   }

   includedirs
   {
      "Source",

	  -- Include Core
	  "../Controller-Deck-Core/Source"
   }

   links
   {
      "Controller-Deck-Core"
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

   filter "system:windows"
       systemversion "latest"
       defines {
           "_WIN32_WINNT=0X0A00",
           "WIN32_LEAN_AND_MEAN",
           "NOMINMAX",
           "ASIO_STANDALONE"
       }
       buildoptions { "/utf-8"} -- richiesto da ftm per i file di risorse
       -- librerie di sistema per asio + SetupDi* (enumerazione COM)
        links { "setupapi", "ws2_32", "mswsock", "advapi32", "ole32", "uuid", "mmdevapi" }
    filter{}

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       kind "WindowedApp"                 -- niente console in Release
       linkoptions { "/ENTRY:mainCRTStartup" }  -- usa main() come entry GUI
       flags { "LinkTimeOptimization" }
       runtime "Release"
       optimize "Full"
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       runtime "Release"
       optimize "On"
       symbols "Off"