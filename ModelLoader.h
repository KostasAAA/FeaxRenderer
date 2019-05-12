#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <vector>
#include <d3d11_1.h>
#include <DirectXMath.h>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

#include "Renderables/Mesh.h"
//#include "TextureLoader.h"

using namespace DirectX;
using namespace std;

class Model;

class ModelLoader
{
public:
	ModelLoader();
	~ModelLoader();

	Model* Load(ID3D12Device* device, std::string& filename);

	void Close();
private:
	ID3D11Device *dev;
	ID3D11DeviceContext *devcon;
	string directory;
	//vector<Texture> textures_loaded;
	HWND hwnd;

	void processNode(ID3D12Device* device, aiNode* node, const aiScene* scene, Model* model);
	Mesh* processMesh(ID3D12Device* device, aiMesh* mesh, const aiScene* scene);
	void loadMaterialTextures(vector<Texture*>& textures, aiMaterial * mat, string& textype, aiTextureType type, string typeName, const aiScene * scene);

	string determineTextureType(const aiScene* scene, aiMaterial* mat);
	int getTextureIndex(aiString* str);

	//	ID3D11ShaderResourceView* getTextureFromModel(const aiScene* scene, int textureindex);
};

#endif // !MODEL_LOADER_H

