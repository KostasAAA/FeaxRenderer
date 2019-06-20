
#pragma once

class DescriptorHeapManager;
class Scene;
class Buffer;
class Rendertarget;

struct GraphicsContext
{
	Scene*						m_scene;
	ID3D12Device*				m_device;
	ID3D12GraphicsCommandList*	m_commandList;
	DescriptorHeapManager*		m_descriptorManager;

	//add them here for the moment, need a better way to pass them around
	Buffer*						m_shadowsCB;
	Rendertarget*				m_shadowsRT;

	unsigned int				m_frameIndex;

	std::vector<CD3DX12_RESOURCE_BARRIER> m_barriers;
};