# Lumine
Lumine is an early-stage C++ based renderer for Windows built with DirectX 12 API. The basic framework is heavily based on Frank D. Luna's DirectX 12 book. For more details, please refer to the ``References`` section.

## Getting Started

Visual Studio 2019 is recommended. Start by cloning the repository with `git clone --recursive https://github.com/AmanThuL/lumine.git`.

If the repository was cloned __non-recursively__ previously, use `git submodule update --init` to clone the necessary submodules.

The project files are regenerable by using ``GenerateProjectFiles.bat``.

## Structure
```
├───Engine              // Engine framework 
│   ├───Engine            // Engine source code
|   |   ├───Common          // Common engine code
|   |   ├───GUI             // GUI integration
|   |   └───Shaders         // HLSL shaders
│   ├───Resources         // Resource files (ex. models, textures, fonts, etc.) 
│   └───Externals           // Third party libs (ex. ImGui, etc.)
├───Projects            // Projects
|   ├───LunaDemoScenes    // Two demo scenes provided in Frank D. Luna's tutorial book
|   └───Shadows           // Demo scene to experiment with different real-time shadow techniques
└───Externals           // Premake 5 executable and a util file
```

## References
- [Introduction to 3D Game Programming with DirectX 12](http://www.d3dcoder.net/d3d12.htm)
- [ImGui](https://github.com/ocornut/imgui)

## Screenshots
Shadow mapping with PCF filtering (9 samples)
![Shadows](https://user-images.githubusercontent.com/42753361/112419564-0055b500-8d02-11eb-8d05-dd4d739ef18e.png)
