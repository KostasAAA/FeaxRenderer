#pragma once

#include<vector>
#include<map>
#include<string>

struct Line
{
	XMFLOAT3 m_startPoint;
	XMFLOAT3 m_endPoint;
	XMFLOAT3 m_colour;
};

class DebugRenderer
{
public:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 colour;
	};

	DebugRenderer() {}
	virtual ~DebugRenderer() {};

	void Initialise(std::wstring& assetsPath, DXGI_FORMAT rtFormat);
	void Begin();
	void End();
	void AddLine(XMFLOAT3& start, XMFLOAT3& end, XMFLOAT3& colour);

	void Render(Buffer* cb, GPUDescriptorHeap* gpuDescriptorHeap);

private:

	std::vector<Line> m_lines;

	GraphicsPSO		m_pso;
	RootSignature	m_rs;

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};


