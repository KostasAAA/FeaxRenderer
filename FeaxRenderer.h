//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DXSample.h"
#include "Resources\PipelineState.h"
#include "Resources\RootSignature.h"
#include "Renderables\Mesh.h"
#include "Renderables\Model.h"
#include "Resources\DescriptorHeap.h"
#include "RendertargetManager.h"
#include "TextureManager.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class ModelLoader;
class Mesh;
class Texture;
class Rendertarget;
class Model;
class GraphicsPSO;
class RootSignature;
class DescriptorHandle;
class Scene;
class RendertargetManager;

class FeaxRenderer : public DXSample
{
public:
	struct GBuffer
	{
		enum Enum 
		{
			Albedo,
			Normal,
			Noof
		};
	};

	struct Lighting
	{
		enum Enum
		{
			Diffuse,
			Specular,
			Noof
		};
	};


    FeaxRenderer(UINT width, UINT height, std::wstring name);

	~FeaxRenderer();

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

private:

	__declspec(align(16)) struct GPrepassCBData
	{
		XMMATRIX ViewProjection;
	};

	__declspec(align(16)) struct LightPassCBData
	{
		XMMATRIX InvViewProjection;
		XMFLOAT4 LightDirection;
		XMFLOAT4 CameraPos;
		XMFLOAT4 RTSize;
	};

	__declspec(align(16)) struct ShadowPassCBData
	{
		XMMATRIX	ViewProjection;
		XMMATRIX	InvViewProjection;
		XMFLOAT4	RTSize;
		XMFLOAT4	LightDir;
		XMFLOAT4	CameraPos;
	};

	__declspec(align(16)) struct SSRCBData
	{
		XMMATRIX	ViewProjection;
		XMMATRIX	InvViewProjection;
		XMMATRIX	Projection;
		XMMATRIX	InvProjection;
		XMMATRIX	View;
		XMMATRIX	InvView;

		XMFLOAT4	RTSize;
		XMFLOAT3	CameraPos;
		float		SSRScale;
		XMFLOAT4	LightDirection;

		float		ZThickness; // thickness to ascribe to each pixel in the depth buffer
		float		NearPlaneZ; // the camera's near z plane
		float		FarPlaneZ; // the camera's far z plane
		float		Stride; // Step in horizontal or vertical pixels between samples. This is a float
							// because integer math is slow on GPUs, but should be set to an integer >= 1.
		float		MaxSteps; // Maximum number of iterations. Higher gives better images but may be slow.
		float		MaxDistance; // Maximum camera-space distance to trace before returning a miss.
		float		StrideZCutoff;	// More distant pixels are smaller in screen space. This value tells at what point to
									// start relaxing the stride to give higher quality reflections for objects far from
									// the camera.
		float		ReflectionsMode;
	};

	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_depthStencil;

	RendertargetManager	m_rendertargetManager;
	TextureManager		m_textureManager;
	
	//GBuffer pass
	Rendertarget*	m_gbufferRT[GBuffer::Noof];
	GraphicsPSO		m_gbufferPSO;
	RootSignature	m_gbufferRS;
	Buffer*			m_gbufferCB;

	//raytraced shadows pass
	Rendertarget*	m_shadowsRT;
	Rendertarget*	m_shadowsHistoryRT;
	ComputePSO		m_shadowsPSO;
	RootSignature	m_shadowsRS;
	Buffer*			m_shadowsCB;
	ShadowPassCBData m_shadowsCBData;
	Texture*		m_blueNoiseTexture;

	//Lighting pass
	Rendertarget*	m_lightingRT[Lighting::Noof];
	GraphicsPSO		m_lightingPSO;
	RootSignature	m_lightingRS;
	Buffer*			m_lightingCB;

	//composite pass
	Rendertarget*	m_mainRT;
	GraphicsPSO		m_mainPSO;
	RootSignature	m_mainRS;

	//SSR pass
	Rendertarget*	m_ssrRT;
	ComputePSO		m_ssrPSO;
	RootSignature	m_ssrRS;
	Buffer*			m_ssrCB;
	SSRCBData		m_ssrCBData;

	//tonemapping pass
	Rendertarget*	m_backbuffer[Graphics::FrameCount];
	GraphicsPSO		m_tonemappingPSO;
	RootSignature	m_tonemappingRS;
	bool			m_SSRDebug;

	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12DescriptorHeap>  m_UISrvDescHeap;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	ComPtr<ID3D12Resource> m_fullscreenVertexBuffer;
	ComPtr<ID3D12Resource> m_fullscreenVertexBufferUpload;
	D3D12_VERTEX_BUFFER_VIEW m_fullscreenVertexBufferView;

	std::vector<Material>	m_materials;
	Buffer*					m_materialBuffer;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

	void LoadMeshes();
	void CreateRenderpassResources();
    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
	void GenerateCheckerboardTextureData(std::vector<UINT8>& data, UINT width, UINT height, UINT bytesPerTexel);
	void SetupImGUI();
};
