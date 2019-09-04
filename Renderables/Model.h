#pragma once

#include "..\Resources\RootSignature.h"
#include "..\Resources\Buffer.h"

class Mesh;
class Buffer;
struct IDxcBlob;
class GPUDescriptorHeap;

struct Material
{
	int			m_albedoID;
	int			m_normalID;
	float		m_roughness;
	float		m_metalness;
	XMFLOAT2	m_uvScale;
	XMFLOAT2	m_normalScale;

	Material() : m_albedoID(-1), m_normalID(-1), m_roughness(0.5f), m_metalness(0), m_uvScale(XMFLOAT2(0, 0)), m_normalScale(XMFLOAT2(1, 1)) {}
};

class Model
{
public:

	Model() {};
	Model(Mesh* mesh) : m_blasBuffer(nullptr){ AddMesh(mesh); }
	~Model();

	void Render(ID3D12GraphicsCommandList* commandList);

	void AddMesh(Mesh* mesh)
	{
		if (mesh != nullptr)
		{
			m_aabox.Expand(mesh->GetAABB());
			m_meshes.push_back(mesh);
		}
	}

	std::vector<Mesh*>& GetMeshes()
	{
		return m_meshes;
	}

	Mesh* GetMesh(int index = 0)
	{
		if (index < m_meshes.size())
			return m_meshes[index];

		return nullptr;
	}

	AABB& GetAABB() { return m_aabox; }

	void SetBlasBuffer(Buffer* buffer) { m_blasBuffer = buffer; }
	Buffer* GetBlasBuffer() { return m_blasBuffer;  }

private:
	AABB				m_aabox;
	std::vector<Mesh*>	m_meshes;

	Buffer*				m_blasBuffer;
};

class ModelInstance
{
public:
	__declspec(align(16)) struct ModelInstanceCBData
	{
		XMMATRIX	World;
		float		AlbedoID;
		float		NormalID;
		float		Roughness;
		float		Metalness;
		XMFLOAT2	UVscale;
		XMFLOAT2	Normalscale;
	};

	ModelInstance(Model* model, Material& material, int materialID, XMMATRIX& objectToWorld) : 
		m_model(model), 
		m_objectToWorld(objectToWorld), 
		m_material(material), 
		m_materialID(materialID)
	{ 
		Buffer::Description desc;
		desc.m_elementSize = sizeof(ModelInstanceCBData);
		desc.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
		desc.m_descriptorType = Buffer::DescriptorType::CBV;

		m_modelInstanceCB = new Buffer(desc, L"model instance CB");

		Update(); 
	}

	~ModelInstance() { delete m_modelInstanceCB ; }

	Model* GetModel() { return m_model; }
	XMMATRIX& GetMatrix() { return m_objectToWorld; }
	AABB& GetAABB() { return m_aabox; }
	Material& GetMaterial() { return m_material; }
	int GetMaterialID() { return m_materialID; }

	Buffer* GetCB() { return m_modelInstanceCB; }

	void Update() 
	{ 
		XMVECTOR minBounds = Float3ToVector4(m_model->GetAABB().MinBounds);
		XMVECTOR maxBounds = Float3ToVector4(m_model->GetAABB().MaxBounds);

		m_aabox.MinBounds = Vector4ToFloat3(XMVector4Transform(minBounds, m_objectToWorld));
		m_aabox.MaxBounds = Vector4ToFloat3(XMVector4Transform(maxBounds, m_objectToWorld));

		ModelInstanceCBData cbData = {};
		cbData.World = m_objectToWorld;
		cbData.AlbedoID = m_material.m_albedoID;
		cbData.NormalID = m_material.m_normalID;
		cbData.Roughness = m_material.m_roughness;
		cbData.Metalness = m_material.m_metalness;
		cbData.UVscale = m_material.m_uvScale;
		cbData.Normalscale = m_material.m_normalScale;

		memcpy(m_modelInstanceCB->Map(), &cbData, sizeof(cbData));
	}

private:
	Material	m_material;
	XMMATRIX	m_objectToWorld;
	Model*		m_model;
	AABB		m_aabox;
	Buffer*		m_modelInstanceCB;
	int			m_materialID;
};

class Scene
{
public:
	struct DXR
	{
		Buffer*			m_tlasBuffer;
		RootSignature	m_raygenRS;
		RootSignature	m_closestHitRS;
		RootSignature	m_missRS;
		IDxcBlob*		m_raygenBlob;
		IDxcBlob*		m_closestHitBlob;
		IDxcBlob*		m_missBlob;

		Buffer*			m_shaderTableBuffer;
		uint			m_shaderTableEntrySize;
		
		GPUDescriptorHeap* m_descriptorHeap;

		ID3D12StateObject*	m_rtPSO;
		ID3D12StateObjectProperties* m_rtpsoInfo;
	};

	Scene() {}
	~Scene() {}

	std::vector<ModelInstance*>& GetModelInstances()
	{
		return m_modelInstances;
	}

	std::vector<Model*>& GetModels()
	{
		return m_models;
	}

	void AddModelInstance(ModelInstance* modelInstance)
	{
		m_aabox.Expand(modelInstance->GetAABB());
		m_modelInstances.push_back(modelInstance);

		std::vector<Model*>::iterator it = find(m_models.begin(), m_models.end(), modelInstance->GetModel());
		if (it == m_models.end())
		{
			m_models.push_back(modelInstance->GetModel());
		}
	}

	AABB& GetAABB() { return m_aabox; }

	void SetBVHBuffer(Buffer* bvhBuffer, Buffer* bvhBufferNormals, Buffer* bvhBufferUVs) 
	{ 
		m_bvhBuffer = bvhBuffer;
		m_bvhBufferNormals = bvhBufferNormals;
		m_bvhBufferUVs = bvhBufferUVs;
	}

	Buffer* GetBVHBuffer() { return m_bvhBuffer; }
	Buffer* GetBVHBufferNormals() { return m_bvhBufferNormals; }
	Buffer* GetBVHBufferUVs() { return m_bvhBufferUVs; }

	void SetTLASBuffer(Buffer* buffer) { m_dxr.m_tlasBuffer = buffer; }
	Buffer* GetTLASBuffer() { return m_dxr.m_tlasBuffer; }

	DXR& GetDXRData() { return m_dxr; }

private:
	AABB						m_aabox;
	std::vector<ModelInstance*> m_modelInstances;
	std::vector<Model*>			m_models;
	Buffer*						m_bvhBuffer;
	Buffer*						m_bvhBufferNormals;
	Buffer*						m_bvhBufferUVs;

	DXR							m_dxr;
};


