// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

//#define DXR_ENABLED

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <wrl.h>
#include <shellapi.h>

#include <vector>

#include "DXSample.h"

#include "Maths.h"
#include "GraphicsContext.h"
#include "Resources\Graphics.h"
#include "GPUProfiler.hpp"

#if defined DXR_ENABLED
//#include <dxc/dxcapi.use.h>
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;

inline void ThrowError(const char* message)
{
	throw std::runtime_error(message);
}
