#include "stdafx.h"

#include "imgui\imgui.h"
#include "imgui\imgui_impl_win32.h"
#include "imgui\imgui_impl_dx12.h"

#include "ModelLoader.h"
#include "Resources\Graphics.h"
#include "Resources\DescriptorHeap.h"
#include "Resources\Texture.h"
#include "Resources\Rendertarget.h"
#include "Resources\Buffer.h"
#include "Renderables\Model.h"
#include "BVH.h"

#include "FeaxRenderer.h"

using namespace Graphics;

FeaxRenderer::FeaxRenderer(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_frameIndex(0)
{
}

void FeaxRenderer::OnInit()
{
	Graphics::InitializeCommonState();

    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void FeaxRenderer::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ID3D12Debug* debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

			debugController->Release();
        }
    }
#endif

    IDXGIFactory4* factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        IDXGIAdapter* warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&Graphics::Context.m_device)
            ));

		warpAdapter->Release();
    }
    else
    {
        IDXGIAdapter1* hardwareAdapter;
        GetHardwareAdapter(factory, &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));

		hardwareAdapter->Release();
    }

	factory->Release();

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    IDXGISwapChain1* swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue,        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    m_swapChain = (IDXGISwapChain3*)swapChain;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	Graphics::Context.m_device = m_device;
	Graphics::Context.m_descriptorManager = new DescriptorHeapManager();

	// Create descriptor heaps.
    {

		// Describe and create a depth stencil view (DSV) descriptor heap.
		//D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		//dsvHeapDesc.NumDescriptors = 1;
		//dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		//dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		//ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		// Describe and create a constant buffer view (CBV) descriptor heap.
		// Flags indicate that this descriptor heap can be bound to the pipeline 
		// and that descriptors contained in it can be referenced by a root table.
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.NumDescriptors = 1;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
    }

    // Create frame resources.
    {
		m_backbuffer = new Rendertarget( m_swapChain, L"Backbuffer");
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	SetupImGUI();
}

// Load the sample assets.
void FeaxRenderer::LoadAssets()
{    
	Graphics::Context.m_device = m_device;

	ModelLoader modelLoader;

	// Create the depth buffer and views.
	{
		// Describe and create a depth stencil view (DSV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
		));

		m_device->CreateDepthStencilView(m_depthStencil, &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		m_device->CreateShaderResourceView(m_depthStencil, &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	//create resources for g-buffer pass 
	{
		XMFLOAT4 clearColour = { 0.0f, 0.5f, 1.0f, 0.0f };
		XMFLOAT4 clearNormal = { 0.0f, 0.0f, 0.0f, 0.0f };

		//rendertargets
		m_gbufferRT[GBuffer::Albedo] = new Rendertarget(m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, FrameCount, clearColour, L"GBuffer RT");
		m_gbufferRT[GBuffer::Normal] = new Rendertarget( m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, FrameCount, clearNormal, L"Normals RT");

		// root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		m_gBUfferRS.Reset(2, 1);
		m_gBUfferRS.InitStaticSampler(0, SamplerLinearWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_gBUfferRS[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_gBUfferRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_gBUfferRS.Finalise(m_device, L"GPrepassRS", rootSignatureFlags);

		//Create Pipeline State Object
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\GBufferPass.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\GBufferPass.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		DXGI_FORMAT m_rtFormats[GBuffer::Noof];
		for (int i = 0; i < GBuffer::Noof; i++)
			m_rtFormats[i] = m_gbufferRT[i]->GetFormat();

		m_gbufferPSO.SetRootSignature(m_gBUfferRS);
		m_gbufferPSO.SetRasterizerState(RasterizerDefault);
		m_gbufferPSO.SetBlendState(BlendDisable);
		m_gbufferPSO.SetDepthStencilState(DepthStateReadWrite);
		m_gbufferPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		m_gbufferPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_gbufferPSO.SetRenderTargetFormats(_countof(m_rtFormats), m_rtFormats, DXGI_FORMAT_D32_FLOAT);
		m_gbufferPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		m_gbufferPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		m_gbufferPSO.Finalize(m_device);
	}

	//create resources for raytraced shadows pass
	{
		XMFLOAT4 clearColour = { 0,0,0,0 };

		//create rendertargets
		m_shadowsRT = new Rendertarget(m_width, m_height, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, FrameCount, clearColour, L"Raytraced Shadows RT");
		m_shadowsHistoryRT = new Rendertarget(m_width, m_height, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, FrameCount, clearColour, L"Raytraced Shadows History RT");

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		m_shadowsRS.Reset(3, 0);
		m_shadowsRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		m_shadowsRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_ALL);
		m_shadowsRS[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		m_shadowsRS.Finalise(m_device, L"Raytraced shadows pass RS", rootSignatureFlags);

		//create pipeline state object
		ComPtr<ID3DBlob> computeShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\RaytracedShadowsPass.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, nullptr));

		m_shadowsPSO.SetRootSignature(m_shadowsRS);
		m_shadowsPSO.SetComputeShader(computeShader->GetBufferPointer(), computeShader->GetBufferSize());
		m_shadowsPSO.Finalize(m_device);
	}

	//create resources for lighting pass
	{
		XMFLOAT4 clearColour = { 0,0,0,0 };

		//create rendertargets
		m_lightingRT[Lighting::Diffuse] = new Rendertarget( m_width, m_height, DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, FrameCount, clearColour, L"Light Diffuse RT");
		m_lightingRT[Lighting::Specular] = new Rendertarget( m_width, m_height, DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, FrameCount, clearColour, L"Light Specular RT");

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		m_lightingRS.Reset(2, 0);
		m_lightingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
		m_lightingRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_PIXEL);
		m_lightingRS.Finalise(m_device, L"Lighting pass RS", rootSignatureFlags);

		//create pipeline state object

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\LightingPass.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\LightingPass.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		DXGI_FORMAT m_rtFormats[Lighting::Noof];
		for (int i = 0; i < Lighting::Noof; i++)
			m_rtFormats[i] = m_lightingRT[i]->GetFormat();

		m_lightingPSO.SetRootSignature(m_lightingRS);
		m_lightingPSO.SetRasterizerState(RasterizerDefault);
		m_lightingPSO.SetBlendState(BlendDisable);
		m_lightingPSO.SetDepthStencilState(DepthStateDisabled);
		m_lightingPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		m_lightingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_lightingPSO.SetRenderTargetFormats(_countof(m_rtFormats), m_rtFormats, DXGI_FORMAT_D32_FLOAT);
		m_lightingPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		m_lightingPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		m_lightingPSO.Finalize(m_device);
	}

	//create resources for composite pass
	{
		XMFLOAT4 clearColour = { 0,0,0,0 };

		//create rendertargets
		m_mainRT = new Rendertarget( m_width, m_height, DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, FrameCount, clearColour, L"Light Diffuse RT");

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		m_mainRS.Reset(1, 0);
		m_mainRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3, D3D12_SHADER_VISIBILITY_PIXEL);
		m_mainRS.Finalise(m_device, L"Composite pass RS", rootSignatureFlags);

		//create pipeline state object

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\CompositePass.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\CompositePass.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		m_mainPSO = m_lightingPSO;

		m_mainPSO.SetRootSignature(m_mainRS);
		m_mainPSO.SetRenderTargetFormat(m_mainRT->GetFormat(), DXGI_FORMAT_D32_FLOAT);
		m_mainPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		m_mainPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		m_mainPSO.Finalize(m_device);
	}

	//create resources for postprocessing pass
	{
		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		m_tonemappingRS.Reset(1, 0);
		m_tonemappingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_tonemappingRS.Finalise(m_device, L"Tonemapping pass RS", rootSignatureFlags);

		//create pipeline state object

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\TonemappingPass.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"Assets\\Shaders\\TonemappingPass.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		m_tonemappingPSO = m_mainPSO;

		m_tonemappingPSO.SetRootSignature(m_tonemappingRS);
		m_tonemappingPSO.SetRenderTargetFormat(m_backbuffer->GetFormat(), DXGI_FORMAT_D32_FLOAT);
		m_tonemappingPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		m_tonemappingPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		m_tonemappingPSO.Finalize(m_device);
	}
	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator, m_gbufferPSO.GetPipelineStateObject(), IID_PPV_ARGS(&m_commandList)));

	Graphics::Context.m_commandList = m_commandList;

	//load meshes
	{
		Model* model = modelLoader.Load(m_device, string("Assets\\Meshes\\statue.obj"));
		XMMATRIX objectToWorld = XMMatrixTranslationFromVector(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f));

		m_scene.AddModelInstance(new ModelInstance(model, objectToWorld));

		model = new Model(Mesh::CreatePlane(m_device));

		objectToWorld = XMMatrixScaling(2, 1, 2);

		m_scene.AddModelInstance(new ModelInstance(model, objectToWorld));

		BVH::CreateBVHBuffer(&m_scene);
	}

	//create full triangle resources
	{
		struct FullscreenVertex
		{
			XMFLOAT3 position;
			XMFLOAT2 uv;
		};

		FullscreenVertex fullscreenTriangleVertices[] =
		{
			// 1 triangle that fills the entire render target.

			{ { -1.0f, -3.0f, 0.0f },{ 0.0f, 2.0f } },    // Bottom left
			{ { -1.0f, 1.0f, 0.0f },{ 0.0f, 0.0f } },    // Top left
			{ { 3.0f, 1.0f, 0.0f },{ 2.0f, 0.0f } },    // Top right
		};

		int vertexBufferSize = 3 * sizeof(FullscreenVertex);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr,
			IID_PPV_ARGS(&m_fullscreenVertexBuffer)));

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_fullscreenVertexBufferUpload)));

		// Copy data to the intermediate upload heap. It will be uploaded to the
		// DEFAULT buffer when the color space triangle vertices are updated.
		UINT8* mappedUploadHeap = nullptr;
		ThrowIfFailed(m_fullscreenVertexBufferUpload->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&mappedUploadHeap)));

		memcpy(mappedUploadHeap, fullscreenTriangleVertices, vertexBufferSize);

		m_fullscreenVertexBufferUpload->Unmap(0, &CD3DX12_RANGE(0, 0));

		m_fullscreenVertexBufferView.BufferLocation = m_fullscreenVertexBuffer->GetGPUVirtualAddress();
		m_fullscreenVertexBufferView.StrideInBytes = sizeof(FullscreenVertex);
		m_fullscreenVertexBufferView.SizeInBytes = vertexBufferSize;

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_fullscreenVertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST));
		m_commandList->CopyBufferRegion(m_fullscreenVertexBuffer, 0, m_fullscreenVertexBufferUpload, 0, m_fullscreenVertexBuffer->GetDesc().Width);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_fullscreenVertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	}

	m_texture = new Texture("Assets\\Textures\\spnza_bricks_a_diff.jpg", m_device, m_commandList);
	m_texture2 = new Texture("Assets\\Textures\\sand_texture.jpg", m_device, m_commandList);

	// Create the constant buffers.
	{
		{
			Buffer::Description desc;
			desc.m_elementSize = sizeof(SceneConstantBuffer);
			desc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.m_descriptorType = Buffer::DescriptorType::CBV;

			m_constantBuffer = new Buffer(desc, L"Scene Constant Buffer");
		}

		//create constant buffer for shadowpass
		{
			Buffer::Description desc;
			desc.m_elementSize = sizeof(ShadowPassCBData);
			desc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
			desc.m_descriptorType = Buffer::DescriptorType::CBV;

			m_shadowsCB = new Buffer(desc, L"Raytraced Shadows CB");
		}
	}

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();
    }

}

bool show_demo_window;
bool show_another_window;

inline bool IsKeyPressed(int key)
{
	return (GetKeyState(key) & 0x8000) != 0;
}

// Update frame-based values.
void FeaxRenderer::OnUpdate()
{
	//capture mouse clicks
	ImGui::GetIO().MouseDown[0] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
	ImGui::GetIO().MouseDown[1] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
	ImGui::GetIO().MouseDown[2] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;

		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}


	static float frameCount = 0;
	static float rot = 0;

	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR cameraPos = XMVectorSet( 7.0f, 1.5f, -7.0f, 0.0f);
	XMVECTOR cameraLookAt = XMVectorSet(0.0f, 0.5f, 0.0f, 0.0f);

	float fieldOfViewY = 3.141592654f / 4.0f;

	XMMATRIX view = XMMatrixLookAtLH(cameraPos, cameraLookAt, up);

	XMMATRIX projection = XMMatrixPerspectiveFovLH(fieldOfViewY, m_width / (float)m_height, 1.0f, 200.0f);

	if (IsKeyPressed('A'))
		rot += 0.04f;
	else if (IsKeyPressed('S'))
		rot -= 0.04f;

	XMMATRIX rotation = XMMatrixRotationY(rot);

		XMVECTOR lightDir = XMVectorSet(1, 1, 1, 0);
	lightDir = XMVector3Normalize(lightDir);

	lightDir = XMVector4Transform(lightDir, rotation);

	XMMATRIX viewProjection = view * projection;

	XMVECTOR determinant;
	XMMATRIX invViewProjection = XMMatrixInverse(&determinant, viewProjection);

	m_constantBufferData.WorldViewProjection =  view * projection;
	m_constantBufferData.LightDir = ToFloat4(lightDir);

	memcpy(m_constantBuffer->Map(), &m_constantBufferData, sizeof(m_constantBufferData));

	ShadowPassCBData shadowPassData;
	shadowPassData.ViewProjection = viewProjection;
	shadowPassData.InvViewProjection = invViewProjection;
	XMStoreFloat4(&shadowPassData.CameraPos, cameraPos);
	shadowPassData.LightDir = ToFloat4(lightDir);
	shadowPassData.RTSize = { (float)m_width, (float)m_height, 1.0f / m_width, 1.0f / m_height };
	shadowPassData.CameraPos.w = (int)frameCount;

	memcpy(m_shadowsCB->Map(), &shadowPassData, sizeof(shadowPassData));

	frameCount++;

}

// Render the scene.
void FeaxRenderer::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void FeaxRenderer::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);

	delete m_texture;
	delete m_texture2;

	m_swapChain->Release();
	m_device->Release();
	m_depthStencil->Release();

	delete m_backbuffer;
	for ( int i = 0 ; i < GBuffer::Noof;i++)
		delete m_gbufferRT[i];

	for (int i = 0; i < Lighting::Noof; i++)
		delete m_lightingRT[i];

	m_commandAllocator->Release();
	m_commandQueue->Release();
	m_dsvHeap->Release();
	m_cbvHeap->Release();
	m_srvHeap->Release();
	//m_pipelineStateGPrepass->Release();
	//m_pipelineStateCopyRT->Release();
	m_commandList->Release();
	//m_rootSignature->Release();
	//m_copyRTRootSignature->Release();

	m_fullscreenVertexBuffer->Release();
	m_fullscreenVertexBufferUpload->Release();

	delete m_constantBuffer;
	delete m_shadowsCB;

	m_UISrvDescHeap->Release();

	delete Graphics::Context.m_descriptorManager;
	Graphics::Context.m_descriptorManager = nullptr;

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_fence->Release();
}

void FeaxRenderer::PopulateCommandList()
{
	Graphics::Context.m_device = m_device;
	Graphics::Context.m_commandList = m_commandList;
	Graphics::Context.m_frameIndex = m_frameIndex;

	DescriptorHeapManager* descriptorManager = Graphics::Context.m_descriptorManager;

    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator, m_gbufferPSO.GetPipelineStateObject()));

	GPUDescriptorHeap* gpuDescriptorHeap = descriptorManager->GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescriptorHeap->Reset();

	ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };
	
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// Record commands.
	{
		//gbuffer pass
		{
			m_commandList->SetGraphicsRootSignature(m_gBUfferRS.GetSignature());

			DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(1);

			gpuDescriptorHeap->AddToHandle(srvHandle, m_texture->GetSRV());

			m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetResource()->GetGPUVirtualAddress());
			m_commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			m_commandList->RSSetViewports(1, &m_viewport);
			m_commandList->RSSetScissorRects(1, &m_scissorRect);

			//transition buffers to rendertarget outputs
			ResourceBarrierBegin(Graphics::Context);
			m_gbufferRT[GBuffer::Albedo]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_gbufferRT[GBuffer::Normal]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_backbuffer->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_lightingRT[Lighting::Diffuse]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_lightingRT[Lighting::Specular]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_mainRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_shadowsRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			m_shadowsHistoryRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ResourceBarrierEnd(Graphics::Context);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
			{
				m_gbufferRT[GBuffer::Albedo]->GetRTV().GetCPUHandle(),
				m_gbufferRT[GBuffer::Normal]->GetRTV().GetCPUHandle()
			};

			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
			m_commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &dsvHandle);

			const float clearColor[] = { 0.0f, 0.5f, 1.0f, 0.0f };
			const float clearNormal[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_commandList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
			m_commandList->ClearRenderTargetView(rtvHandles[1], clearNormal, 0, nullptr);
			m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			m_scene.GetModelInstances()[0]->GetModel()->Render(m_commandList);

			srvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(srvHandle, m_texture2->GetSRV());

			m_commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			m_scene.GetModelInstances()[1]->GetModel()->Render(m_commandList);

			//transition rendertargets to shader resources 
			ResourceBarrierBegin(Graphics::Context);
			m_gbufferRT[GBuffer::Albedo]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_gbufferRT[GBuffer::Normal]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ResourceBarrierEnd(Graphics::Context);
		}

		//raytraced shadows pass
		{
			m_commandList->SetPipelineState(m_shadowsPSO.GetPipelineStateObject());
			m_commandList->SetComputeRootSignature(m_shadowsRS.GetSignature());

			Buffer *bvh = m_scene.GetBVHBuffer();

			DescriptorHandle cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(cbvHandle, m_shadowsCB->GetCBV());

			DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(4);
			gpuDescriptorHeap->AddToHandle(srvHandle, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_gbufferRT[GBuffer::Normal]->GetSRV());
			gpuDescriptorHeap->AddToHandle(srvHandle, bvh->GetSRV());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_shadowsHistoryRT->GetSRV());

			DescriptorHandle uavHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(srvHandle, m_shadowsRT->GetUAV());

			m_commandList->SetComputeRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			m_commandList->SetComputeRootDescriptorTable(1, srvHandle.GetGPUHandle());
			m_commandList->SetComputeRootDescriptorTable(2, uavHandle.GetGPUHandle());

			const uint32_t threadGroupSizeX = m_width / 8 + 1;
			const uint32_t threadGroupSizeY = m_height / 8 + 1;

			m_commandList->Dispatch(threadGroupSizeX, threadGroupSizeY, 1);

			ResourceBarrierBegin(Graphics::Context);
			m_shadowsRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_COPY_SOURCE);
			m_shadowsHistoryRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_COPY_DEST);			
			ResourceBarrierEnd(Graphics::Context);

			m_commandList->CopyResource(m_shadowsHistoryRT->GetResource(), m_shadowsRT->GetResource());

			ResourceBarrierBegin(Graphics::Context);
			m_shadowsRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ResourceBarrierEnd(Graphics::Context);
		}

		//lighting pass
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
			{
				m_lightingRT[Lighting::Diffuse]->GetRTV().GetCPUHandle(),
				m_lightingRT[Lighting::Specular]->GetRTV().GetCPUHandle(),
			};

			m_commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, nullptr);

			const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };

			m_commandList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
			m_commandList->ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);

			m_commandList->SetPipelineState(m_lightingPSO.GetPipelineStateObject());
			m_commandList->SetGraphicsRootSignature(m_lightingRS.GetSignature());

			DescriptorHandle cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(cbvHandle, m_constantBuffer->GetCBV());

			DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(4);

			gpuDescriptorHeap->AddToHandle(srvHandle, m_gbufferRT[GBuffer::Albedo]->GetSRV());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_gbufferRT[GBuffer::Normal]->GetSRV());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_shadowsRT->GetSRV());

			m_commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			m_commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			m_commandList->IASetVertexBuffers(0, 1, &m_fullscreenVertexBufferView);
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			m_commandList->DrawInstanced(3, 1, 0, 0);

			ResourceBarrierBegin(Graphics::Context);
			m_lightingRT[Lighting::Diffuse]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_lightingRT[Lighting::Specular]->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ResourceBarrierEnd(Graphics::Context);
		}

		//composite pass
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
			{
				m_mainRT->GetRTV().GetCPUHandle(),
			};

			m_commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, nullptr);

			m_commandList->SetPipelineState(m_mainPSO.GetPipelineStateObject());
			m_commandList->SetGraphicsRootSignature(m_mainRS.GetSignature());

			DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(3);

			gpuDescriptorHeap->AddToHandle(srvHandle, m_gbufferRT[GBuffer::Albedo]->GetSRV());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_lightingRT[Lighting::Diffuse]->GetSRV());
			gpuDescriptorHeap->AddToHandle(srvHandle, m_lightingRT[Lighting::Specular]->GetSRV());

			m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle.GetGPUHandle());

			m_commandList->IASetVertexBuffers(0, 1, &m_fullscreenVertexBufferView);
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			m_commandList->DrawInstanced(3, 1, 0, 0);

			//this should be done by the last pass that touches the main RT
			ResourceBarrierBegin(Graphics::Context);
			m_mainRT->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ResourceBarrierEnd(Graphics::Context);
		}

		//tonemap and copy to backbuffer
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
			{
				m_backbuffer->GetRTV().GetCPUHandle()
			};

			m_commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, nullptr);

			m_commandList->SetPipelineState(m_tonemappingPSO.GetPipelineStateObject());
			m_commandList->SetGraphicsRootSignature(m_tonemappingRS.GetSignature());
			
			DescriptorHandle srvHandle = gpuDescriptorHeap->GetHandleBlock(1);

			gpuDescriptorHeap->AddToHandle(srvHandle, m_mainRT->GetSRV());

			m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle.GetGPUHandle());

			m_commandList->IASetVertexBuffers(0, 1, &m_fullscreenVertexBufferView);
			m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			m_commandList->DrawInstanced(3, 1, 0, 0);

		}

		//render UI
		{
			m_commandList->SetDescriptorHeaps(1, &m_UISrvDescHeap);
			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList);
		}

		// Indicate that the back buffer will now be used to present.
		ResourceBarrierBegin(Graphics::Context);
		m_backbuffer->TransitionTo(Graphics::Context, D3D12_RESOURCE_STATE_PRESENT);
		ResourceBarrierEnd(Graphics::Context);
	}

    ThrowIfFailed(m_commandList->Close());
}

void FeaxRenderer::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence, fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}


// Generate a simple black and white checkerboard texture.
void FeaxRenderer::GenerateCheckerboardTextureData(std::vector<UINT8>& data , UINT width, UINT height, UINT bytesPerTexel )
{
	const UINT rowPitch = width * bytesPerTexel;
	const UINT cellPitch = rowPitch >> 5;        // The width of a cell in the checkboard texture.
	const UINT cellHeight = width >> 5;    // The height of a cell in the checkerboard texture.
	const UINT textureSize = rowPitch * height;

	UINT8* pData = &data[0];

	for (UINT n = 0; n < textureSize; n += bytesPerTexel)
	{
		UINT x = n % rowPitch;
		UINT y = n / rowPitch;
		UINT i = x / cellPitch;
		UINT j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;        // R
			pData[n + 1] = 0x00;    // G
			pData[n + 2] = 0x00;    // B
			pData[n + 3] = 0xff;    // A
		}
		else
		{
			pData[n] = 0xff;        // R
			pData[n + 1] = 0xff;    // G
			pData[n + 2] = 0xff;    // B
			pData[n + 3] = 0xff;    // A
		}
	}
}

void FeaxRenderer::SetupImGUI()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_UISrvDescHeap)));

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());
	ImGui_ImplDX12_Init(m_device, FrameCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		m_UISrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
		m_UISrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'misc/fonts/README.txt' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);


}