# Lumine

Lumine is an early-stage C++ based renderer for Windows built with DirectX 12. For more details, please refer to the sections below.

## Getting Started

Visual Studio 2019 is recommended. Start by cloning the repository with `git clone --recursive https://github.com/AmanThuL/lumine.git`.

If the repository was cloned __non-recursively__ previously, use `git submodule update --init` to clone the necessary submodules.

The project files are regenerable by using ``GenerateProjectFiles.bat``.

## Screenshots

### Shadow Techniques

#### Shadow mapping with PCF filtering (9 samples)
<img src="https://user-images.githubusercontent.com/42753361/112419564-0055b500-8d02-11eb-8d05-dd4d739ef18e.png" width=70%>

#### Percentage Closer Soft Shadows (PCSS)
<img src="Docs/Images/car1.png" width=70%>


## Structure

<img src="Docs/Images/lumine-folder-structure.svg" width=45%>

## References

### Graphics API Abstraction

The DirectX abstraction is based on a series of open source repositories and tutorials listed below.

- [Introduction to 3D Game Programming with DirectX 12](http://www.d3dcoder.net/d3d12.htm)
- [Falcor by NVIDIA](https://github.com/NVIDIAGameWorks/Falcor)
- [MiniEngine by Microsoft](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine)

### External Libraries

- [ImGui](https://github.com/ocornut/imgui)
