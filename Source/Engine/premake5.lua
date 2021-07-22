project (_PROJECT_ENGINE)
    kind "WindowedApp"
    language "C++"
	cppdialect "C++17"
	staticruntime "off"

    targetdir("%{wks.location}/Build/bin/" .. _OUTPUT_DIR .. "/%{prj.name}")
    objdir("%{wks.location}/Build/bin-int/" .. _OUTPUT_DIR .. "/%{prj.name}")

	files
	{ 
		"**.h", "**.cpp",
	}

	includedirs
    {
		"%{wks.location}/Source/Core",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.assimp}",
    }
	
	links
	{
		"Core"
	}
	
	filter "system:windows"
		systemversion "latest"
	
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