
#pragma once

class DescriptorHeapManager;
class Scene;

struct GraphicsContext
{
	Scene*						m_scene;
	ID3D12Device*				m_device;
	ID3D12GraphicsCommandList*	m_commandList;
	DescriptorHeapManager*		m_descriptorManager;
	unsigned int				m_frameIndex;

	std::vector<CD3DX12_RESOURCE_BARRIER> m_barriers;
};