project "Controller-Deck-Core"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   characterset "Unicode"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   -- ✅ Eccezioni ON anche per la libreria
    exceptionhandling "On"
    defines { "_HAS_EXCEPTIONS=1", "FMT_USE_EXCEPTIONS=1" }

   files { 
       "Source/**.h", 
       "Source/**.cpp",
       "Source/**.hpp"
   }
   includedirs
   {
      "Source"
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
        links { "setupapi", "ws2_32", "mswsock", "advapi32" }
    filter{}

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       runtime "Release"
       optimize "On"
       symbols "Off"