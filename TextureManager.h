#pragma once

#include "Resources\Texture.h"
#include<vector>
#include<map>
#include<string>

class TextureManager
{
public:

	TextureManager() {}
	virtual ~TextureManager();

	int Load(std::string filename);

	std::vector<Texture*>& GetTextures() { return m_textures; }

private:

	std::map<std::string, int> m_textureIDs;
	std::vector<Texture*> m_textures;
};


