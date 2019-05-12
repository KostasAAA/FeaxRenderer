#include "stdafx.h"
#include "Mesh.h"
#include "..\Resources\Texture.h"
#include "..\DXSample.h"

Mesh::Mesh(ID3D12Device* device, std::vector<Mesh::Vertex>& vertices, std::vector<UINT>& indices)
	: m_vertices(std::move(vertices))
	, m_indices(std::move(indices))
{
	for (Mesh::Vertex& vertex : vertices)
	{
		m_aabox.Expand(vertex.position);
	}

	m_noofIndices = static_cast<UINT>(m_indices.size());

	const UINT vertexBufferSize =static_cast<UINT>(m_vertices.size()) * sizeof(Mesh::Vertex);
	const UINT indexBufferSize = m_noofIndices * sizeof(m_indices[0]);

	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);        
	
	ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &m_vertices[0], vertexBufferSize);
	m_vertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = vertexBufferSize;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pIndexDataBegin;

	ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &m_indices[0], indexBufferSize);
	m_indexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_indexBufferView.SizeInBytes = indexBufferSize;
}

Mesh::~Mesh()
{
	m_indexBuffer->Release();
	m_vertexBuffer->Release();
}


Mesh* Mesh::CreatePlane(ID3D12Device* device)
{
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;

	float halfSize = 5.5f;

	vertices.push_back({ XMFLOAT3(-halfSize,0,-halfSize), XMFLOAT3(0,1,0), XMFLOAT2(0,0) });
	vertices.push_back({ XMFLOAT3(halfSize,0,-halfSize), XMFLOAT3(0,1,0), XMFLOAT2(1,0) });
	vertices.push_back({ XMFLOAT3(halfSize,0,halfSize), XMFLOAT3(0,1,0), XMFLOAT2(1,1) });
	vertices.push_back({ XMFLOAT3(-halfSize,0,halfSize), XMFLOAT3(0,1,0), XMFLOAT2(0,1) });

	indices.push_back(2);
	indices.push_back(1);
	indices.push_back(0);

	indices.push_back(0);
	indices.push_back(3);
	indices.push_back(2);

	return new Mesh(device, vertices, indices);
}


