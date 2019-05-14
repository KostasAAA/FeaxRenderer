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

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

private:

	struct SceneConstantBuffer
	{
		XMMATRIX WorldViewProjection;
		XMFLOAT4	LightDir;
	};

	struct ShadowPassCBData
	{
		XMMATRIX	ViewProjection;
		XMMATRIX	InvViewProjection;
		XMFLOAT4	RTSize;
		XMFLOAT4	LightDir;
		XMFLOAT4	CameraPos;
	};

    IDXGISwapChain3* m_swapChain;
    ID3D12Device* m_device;
	ID3D12Resource* m_depthStencil;
	
	//GBuffer pass
	Rendertarget*	m_gbufferRT[GBuffer::Noof];
	GraphicsPSO		m_gbufferPSO;
	RootSignature	m_gBUfferRS;

	//raytraced shadows pass
	Rendertarget*	m_shadowsRT;
	Rendertarget*	m_shadowsHistoryRT;
	ComputePSO		m_shadowsPSO;
	RootSignature	m_shadowsRS;
	Buffer*			m_shadowsCB;
	ShadowPassCBData m_shadowsCBData;

	//Lighting pass
	Rendertarget*	m_lightingRT[Lighting::Noof];
	GraphicsPSO		m_lightingPSO;
	RootSignature	m_lightingRS;

	//composite pass
	Rendertarget*	m_mainRT;
	GraphicsPSO		m_mainPSO;
	RootSignature	m_mainRS;

	//tonemapping pass
	Rendertarget*	m_backbuffer;
	GraphicsPSO		m_tonemappingPSO;
	RootSignature	m_tonemappingRS;

    ID3D12CommandAllocator* m_commandAllocator;
    ID3D12CommandQueue* m_commandQueue;
	ID3D12DescriptorHeap* m_dsvHeap;
	ID3D12DescriptorHeap* m_cbvHeap;
	ID3D12DescriptorHeap* m_srvHeap;
	ID3D12GraphicsCommandList* m_commandList;
	ID3D12DescriptorHeap*  m_UISrvDescHeap;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

//    UINT m_rtvDescriptorSize;

	// App resources.
	Buffer* m_constantBuffer;
	SceneConstantBuffer m_constantBufferData;
	Texture* m_texture;
	Texture* m_texture2;

	Scene	m_scene;

	ID3D12Resource* m_fullscreenVertexBuffer;
	ID3D12Resource* m_fullscreenVertexBufferUpload;
	D3D12_VERTEX_BUFFER_VIEW m_fullscreenVertexBufferView;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ID3D12Fence* m_fence;
    UINT64 m_fenceValue;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
	void GenerateCheckerboardTextureData(std::vector<UINT8>& data, UINT width, UINT height, UINT bytesPerTexel);
	void SetupImGUI();
};
