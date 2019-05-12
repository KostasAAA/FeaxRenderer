#pragma once

#include "DescriptorHeap.h"

class Texture
{
public:

	Texture(int width, int height, int noofChannels, unsigned char *data);
	Texture(const char* filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	Texture() {}
	virtual ~Texture();

	ID3D12Resource* GetResource() { return m_texture; }

	DescriptorHandle& GetSRV() { return m_srvHandle; }

private:
	int m_width;
	int m_height;
	int m_noofChannels;
	unsigned char* m_data;

	ID3D12Resource* m_textureUploadHeap;
	ID3D12Resource* m_texture;
	DescriptorHandle m_srvHandle;

	void CreateResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};


