-- lumine_premake_util.lua
-- utility code shared by premake build scripts

_RENDERER_NAME = "Lumine"
_PROJECT_NAME_1 = "Shadows"
_PROJECT_NAME_2 = "Sandbox"

-- _ACTION is a premake global variable and for our usage will be vs2017, vs2019, etc.
-- Strip "vs" from this string to make a suffix for solution and project files.
_VS_SUFFIX = "_" .. string.gsub(_ACTION, "vs", "")

-- Specify build output directory structure here
_OUTPUT_DIR = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
