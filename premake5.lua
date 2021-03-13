include "vendor/premake/premake_utils/lumine_premake_util.lua"

workspace (_RENDERER_NAME)
    configurations { "Debug", "Release" }
    platforms { "x64" }
    startproject (_PROJECT_NAME_1)

    filter "platforms:x64"
        system "Windows"
        architecture "x64"
		
	flags
	{
		"MultiProcessorCompile"
	}

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["ImGui"] = "%{wks.location}/engine/vendor/imgui"
IncludeDir["Engine"] = "%{wks.location}/engine/engine"

include ("projects/" .. _PROJECT_NAME_1)