#include "stdafx.h"
#include "Resources\Graphics.h"
#include "Resources\DescriptorHeap.h"
#include "TextureManager.h"

TextureManager::~TextureManager()
{
	for (Texture* texture : m_textures)
	{
		delete texture;
	}

	m_textures.clear();
	m_textureIDs.clear();
}

int TextureManager::Load(std::string filename)
{
	ID3D12Device* device = Graphics::Context.m_device;
	ID3D12GraphicsCommandList* commandList = Graphics::Context.m_commandList;
	int id = -1;

	auto texID = m_textureIDs.find(filename);
	if (texID != m_textureIDs.end())
	{
		return texID->second;
	}

	const std::string base("Assets\\Textures\\");
	Texture* texture = new Texture( (base + filename).c_str(), device, commandList);

	id = m_textures.size();

	m_textures.push_back(texture);

	m_textureIDs.insert({ filename, id });

	return id;
}