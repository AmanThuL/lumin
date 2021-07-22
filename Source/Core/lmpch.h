//*******************************************************************
// lmpch.h:
//
// Precompiled header file for lumine.
//*******************************************************************

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
#include <wrl/client.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>  // For ReportLiveObjects.

#include <iostream>

// STL headers
#include <array>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <thread>
#include <random>
#include <unordered_map>
#include <vector>

#include <concrt.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <stdint.h>
#include <ppl.h>
#include <comdef.h>
#include <float.h>

// Assimp header files.
#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/anim.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "Defines.h"
#include "GeometryGenerator.h"
#include "Math/MathHelper.h"
#include "Utils/DDSTextureLoader.h"
