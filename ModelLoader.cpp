#include "stdafx.h"
#include "ModelLoader.h"
#include "Resources/Texture.h"
#include "Renderables/Mesh.h"
#include "Renderables/Model.h"

ModelLoader::ModelLoader()
{
}


ModelLoader::~ModelLoader()
{
}

Model* ModelLoader::Load(ID3D12Device* device, std::string& filename)
{
	std::vector<Mesh*> meshes;
	meshes.clear();

	Assimp::Importer importer;

	const aiScene* pScene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);

	if (pScene == NULL)
		return nullptr;

	this->directory = filename.substr(0, filename.find_last_of('/'));

	this->dev = dev;
	this->hwnd = hwnd;

	Model* model = new Model();
	processNode(device, pScene->mRootNode, pScene, model);

	return model;
}

Mesh* ModelLoader::processMesh(ID3D12Device* device, aiMesh * mesh, const aiScene * scene)
{
	// Data to fill
	std::vector<Mesh::Vertex> vertices;
	std::vector<uint> indices;
	std::vector<Texture*> textures;

	// Walk through each of the mesh's vertices
	for (UINT i = 0; i < mesh->mNumVertices; i++)
	{
		Mesh::Vertex vertex;

		vertex.position.x = mesh->mVertices[i].x;
		vertex.position.y = mesh->mVertices[i].y;
		vertex.position.z = mesh->mVertices[i].z;

		vertex.normal.x = mesh->mNormals[i].x;
		vertex.normal.y = mesh->mNormals[i].y;
		vertex.normal.z = mesh->mNormals[i].z;

		if (mesh->mTextureCoords[0])
		{
			vertex.texcoord.x = (float)mesh->mTextureCoords[0][i].x;
			vertex.texcoord.y = (float)mesh->mTextureCoords[0][i].y;
		}

		vertices.push_back(vertex);
	}

	for (UINT i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];

		for (UINT j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	//if (mesh->mMaterialIndex >= 0)
	//{
	//	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	//	string texType = determineTextureType(scene, material);
	//	vector<Texture*> diffuseMaps;
	//	loadMaterialTextures(diffuseMaps, material, texType, aiTextureType_DIFFUSE, "texture_diffuse", scene);
	//	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	//}

	return new Mesh(device, vertices, indices);
}

void ModelLoader::loadMaterialTextures(vector<Texture*>& textures, aiMaterial * mat, string& textype, aiTextureType type, string typeName, const aiScene * scene)
{
	for (UINT i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);

		// Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
		bool skip = false;
		//for (UINT j = 0; j < textures_loaded.size(); j++)
		//{
		//	if (std::strcmp(textures_loaded[j].path.c_str(), str.C_Str()) == 0)
		//	{
		//		textures.push_back(textures_loaded[j]);
		//		skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)
		//		break;
		//	}
		//}

		if (!skip)
		{   // If texture hasn't been loaded already, load it
			if (textype == "embedded compressed texture")
			{
				int textureindex = getTextureIndex(&str);
//				texture.texture = getTextureFromModel(scene, textureindex);
			}
			else
			{
				string filename = string(str.C_Str());
				filename = directory + '/' + filename;

				//Texture* texture = new Texture(filename.c_str());
				//textures.push_back(texture);
			}
			//texture.type = typeName;
			//texture.path = str.C_Str();
			
			//this->textures_loaded.push_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
		}
	}
}

void ModelLoader::Close()
{
}

void ModelLoader::processNode(ID3D12Device* device, aiNode * node, const aiScene * scene, Model* model)
{
	for (UINT i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		Mesh* newMesh = this->processMesh(device, mesh, scene);
		if (newMesh != nullptr)
			model->AddMesh(newMesh);
	}

	for (UINT i = 0; i < node->mNumChildren; i++)
	{
		this->processNode(device, node->mChildren[i], scene, model);
	}
}


string ModelLoader::determineTextureType(const aiScene * scene, aiMaterial * mat)
{
	aiString textypeStr;
	mat->GetTexture(aiTextureType_DIFFUSE, 0, &textypeStr);
	string textypeteststr = textypeStr.C_Str();
	if (textypeteststr == "*0" || textypeteststr == "*1" || textypeteststr == "*2" || textypeteststr == "*3" || textypeteststr == "*4" || textypeteststr == "*5")
	{
		if (scene->mTextures[0]->mHeight == 0)
		{
			return "embedded compressed texture";
		}
		else
		{
			return "embedded non-compressed texture";
		}
	}
	if (textypeteststr.find('.') != string::npos)
	{
		return "textures are on disk";
	}

	return "";
}

int ModelLoader::getTextureIndex(aiString * str)
{
	string tistr;
	tistr = str->C_Str();
	tistr = tistr.substr(1);
	return stoi(tistr);
}

