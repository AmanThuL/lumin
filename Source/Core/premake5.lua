project (_PROJECT_CORE)
    kind "StaticLib"
    language "C++"
	cppdialect "C++17"
	staticruntime "off"

    targetdir("%{wks.location}/Build/bin/" .. _OUTPUT_DIR .. "/%{prj.name}")
    objdir("%{wks.location}/Build/bin-int/" .. _OUTPUT_DIR .. "/%{prj.name}")

	pchheader "lmpch.h"
	pchsource "lmpch.cpp"

	files	
	{ 
		"**.h", "**.cpp",
	}
	
	defines 
	{ 
		"_CRT_SECURE_NO_WARNINGS" 
	}

	includedirs
    {
        "%{IncludeDir.imgui}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.Core}",
    }

	links
	{
		"ImGui",
	}
	
	filter "system:windows"
		systemversion "latest"
	
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