include "Tools/Premake/solution_items.lua"
include "Tools/Premake/lumine_premake_util.lua"
include "Dependencies.lua"

workspace (_WORKSPACE_NAME)
    platforms { "Win64" }
    startproject (_PROJECT_ENGINE)

    configurations 
	{ 
		"Debug", 
		"Release"
	}
	
	filter "platforms:Win64"
		system "Windows"
		architecture "x86_64"
	
	flags
	{
		"MultiProcessorCompile"
	}

group "Dependencies"
	include "Source/Externals/imgui"
group ""

include ("Source/" .. _PROJECT_CORE)
include ("Source/" .. _PROJECT_ENGINE)
