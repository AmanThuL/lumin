include "Externals/premake/premake_utils/lumine_premake_util.lua"

workspace (_RENDERER_NAME)
    configurations { "Debug", "Release" }
    platforms { "x64" }
    startproject (_PROJECT_NAME_2)

    filter "platforms:x64"
        system "Windows"
        architecture "x64"
		
	flags
	{
		"MultiProcessorCompile"
	}

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["imgui"] = "%{wks.location}/Engine/Externals/imgui"
IncludeDir["assimp"] = "%{wks.location}/Engine/Externals/assimp/include"
IncludeDir["Engine"] = "%{wks.location}/Engine/Engine"

group "Projects"
	include ("Projects/" .. _PROJECT_NAME_1)
