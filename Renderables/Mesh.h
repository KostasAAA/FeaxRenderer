#pragma once

class Texture;

class Mesh
{
public:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 texcoord;
	};

	Mesh(ID3D12Device* device, std::vector<Mesh::Vertex>& vertices, std::vector<UINT>& indices);
	~Mesh();

	static Mesh* CreatePlane(ID3D12Device* device);

	std::vector<Vertex>& GetVertices() { return m_vertices; }
	std::vector<uint>& GetIndices() { return m_indices; }
	AABB& GetAABB() { return m_aabox; }

	uint GetNoofIndices(){ return m_noofIndices;  }

	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

private:
	std::vector<Vertex> m_vertices;
	std::vector<uint>	m_indices;
	AABB				m_aabox;
	uint				m_noofIndices;

	ID3D12Resource* m_vertexBuffer;
	ID3D12Resource* m_indexBuffer;
};


