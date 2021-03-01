project (_PROJECT_NAME_1)
    kind "WindowedApp"
    language "C++"

    targetdir("%{wks.location}/bin/" .. _OUTPUT_DIR .. "/%{prj.name}")
    objdir("%{wks.location}/bin-int/" .. _OUTPUT_DIR .. "/%{prj.name}")

	pchheader "lmpch.h"
	pchsource "src/lmpch.cpp"

	files	
	{ 
		 "src/**.h", "src/**.cpp",
		 "%{IncludeDir.Engine}/**.h", "%{IncludeDir.Engine}/**.cpp",
		 "%{IncludeDir.ImGui}/**.h", "%{IncludeDir.ImGui}/**.cpp",
	}
	
	includedirs
    {
        "src",
        "%{IncludeDir.ImGui}",
		"%{IncludeDir.Engine}"
    }

	links 
	{ 
		"d3dcompiler", 
		"d3d12", 
		"dxgi",
	}
	
	flags { "NoPCH" }

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"
		staticruntime "on"
	
	defines { "_CRT_SECURE_NO_WARNINGS" }
	
    filter "configurations:Debug"
        defines { "WIN32", "_DEBUG", "DEBUG", "_WINDOWS" }
        flags { "FatalWarnings" }
		symbols "On"
		runtime "Debug"

    filter "configurations:Release"
        defines { "WIN32", "NDEBUG", "PROFILE", "_WINDOWS" }
        flags { "LinkTimeOptimization", "FatalWarnings" }
		symbols "On"
		runtime "Release"
        optimize "On"