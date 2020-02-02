#include "stdafx.h"
#include "Resources\Graphics.h"
#include "Resources\DescriptorHeap.h"
#include "Resources\Buffer.h"
#include "DebugRenderer.h"

using namespace Graphics;

void DebugRenderer::Initialise(std::wstring& assetsPath, DXGI_FORMAT rtFormat)
{
	ID3D12Device* device = Graphics::Context.m_device;

	// root signature
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	m_rs.Reset(1, 0);
	m_rs[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	m_rs.Finalise(device, L"debug RS", rootSignatureFlags);

	//Create Pipeline State Object
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ID3DBlob* errorBlob = nullptr;

	ThrowIfFailed(D3DCompileFromFile((assetsPath + L"Assets\\Shaders\\DebugRender.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));

	compileFlags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

	ThrowIfFailed(D3DCompileFromFile((assetsPath + L"Assets\\Shaders\\DebugRender.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorBlob));

	if (errorBlob)
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		errorBlob->Release();
	}

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOUR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_pso.SetRootSignature(m_rs);
	m_pso.SetRasterizerState(RasterizerDefault);
	m_pso.SetBlendState(BlendDisable);
	m_pso.SetDepthStencilState(DepthStateReadWrite);
	m_pso.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
	m_pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	m_pso.SetRenderTargetFormats(1, &rtFormat, DXGI_FORMAT_D32_FLOAT);
	m_pso.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
	m_pso.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
	m_pso.Finalize(device);
}

void DebugRenderer::AddLine(XMFLOAT3& start, XMFLOAT3& end, XMFLOAT3& colour)
{
	m_lines.push_back({ start, end, colour });
}

void DebugRenderer::Begin()
{
	m_lines.clear();
}

void DebugRenderer::End()
{
	std::vector<Vertex> vertices;

	for (Line& line : m_lines)
	{
		vertices.push_back({ line.m_startPoint, line.m_colour });
		vertices.push_back({ line.m_endPoint, line.m_colour });
	}

	ID3D12Device* device = Graphics::Context.m_device;

	const UINT vertexBufferSize = (UINT)vertices.size() * sizeof(Vertex);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* vertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);

	ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
	memcpy(vertexDataBegin, &vertices[0], vertexBufferSize);
	m_vertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
}

void DebugRenderer::Render(Buffer* cb, GPUDescriptorHeap* gpuDescriptorHeap)
{
	ID3D12GraphicsCommandList* commandList = Graphics::Context.m_commandList;

	commandList->SetPipelineState(m_pso.GetPipelineStateObject());
	commandList->SetGraphicsRootSignature(m_rs.GetSignature());

	DescriptorHandle cbvHandle;
	cbvHandle = gpuDescriptorHeap->GetHandleBlock(1);
	gpuDescriptorHeap->AddToHandle(cbvHandle, cb->GetCBV());

	commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	int vertexCount = static_cast<UINT>(2 * m_lines.size());

	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->DrawInstanced(vertexCount, 1, 0, 0);
}
