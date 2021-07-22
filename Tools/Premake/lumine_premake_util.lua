-- lumine_premake_util.lua
-- utility code shared by premake build scripts

_WORKSPACE_NAME = "Lumine"
_PROJECT_CORE = "Core"
_PROJECT_ENGINE = "Engine"

-- _ACTION is a premake global variable and for our usage will be vs2017, vs2019, etc.
-- Strip "vs" from this string to make a suffix for solution and project files.
_VS_SUFFIX = "_" .. string.gsub(_ACTION, "vs", "")

-- Specify build output directory structure here
_OUTPUT_DIR = "%{cfg.buildcfg}-%{cfg.architecture}"
