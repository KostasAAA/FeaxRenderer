#pragma once

class Mesh;
class Buffer;

class Model
{
public:

	Model(ID3D12Device* device);
	Model() {}
	Model(Mesh* mesh) { AddMesh(mesh); }
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

	AABB& GetAABB() { return m_aabox; }

private:
	AABB				m_aabox;
	std::vector<Mesh*>	m_meshes;
};

class ModelInstance
{
public:
	ModelInstance(Model* model, XMMATRIX& objectToWorld) : m_model(model), m_objectToWorld(objectToWorld) { Update(); }

	Model* GetModel() { return m_model; }
	XMMATRIX& GetMatrix() { return m_objectToWorld; }
	AABB& GetAABB() { return m_aabox; }

	void Update() 
	{ 
		XMVECTOR minBounds = Float3ToVector4(m_model->GetAABB().MinBounds);
		XMVECTOR maxBounds = Float3ToVector4(m_model->GetAABB().MaxBounds);

		m_aabox.MinBounds = Vector4ToFloat3(XMVector4Transform(minBounds, m_objectToWorld));
		m_aabox.MaxBounds = Vector4ToFloat3(XMVector4Transform(maxBounds, m_objectToWorld));
	}

private:
	XMMATRIX	m_objectToWorld;
	Model*		m_model;
	AABB		m_aabox;
};

class Scene
{
public:
	Scene() {}
	~Scene() {}

	std::vector<ModelInstance*>& GetModelInstances()
	{
		return m_modelInstances;
	}

	void AddModelInstance(ModelInstance* modelInstance)
	{
		m_aabox.Expand(modelInstance->GetAABB());
		m_modelInstances.push_back(modelInstance);
	}

	AABB& GetAABB() { return m_aabox; }

	void SetBVHBuffer(Buffer* buffer) { m_bvhBuffer = buffer; }
	Buffer* GetBVHBuffer() { return m_bvhBuffer; }

private:
	AABB						m_aabox;
	std::vector<ModelInstance*> m_modelInstances;
	Buffer*						m_bvhBuffer;
};


