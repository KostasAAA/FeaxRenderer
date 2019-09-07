#include "stdafx.h"
#include "BVH.h"
#include "Renderables/Mesh.h"
#include "Renderables/Model.h"
#include "Resources/Buffer.h"
#include <algorithm>
#include <vector>

using namespace std;

namespace BVH
{
	std::vector<XMFLOAT3> m_normalsList;
	std::vector<XMFLOAT2> m_uvList;
	std::vector<int> m_materialIDList;

	void AddPropToPrimitivesAABBList(
		std::vector<BVHAABB>&		bboxData,
		std::vector<BVHAABBCentre>&	bboxCentres,
		Scene*						scene
	)
	{
		int index = 0;

		for (ModelInstance* modelInstance : scene->GetModelInstances())
		{
			XMMATRIX& world = modelInstance->GetMatrix();
			Material& material = modelInstance->GetMaterial();
			int materialID = modelInstance->GetMaterialID();

			Model* model = modelInstance->GetModel();

			for (Mesh* mesh : model->GetMeshes())
			{
				std::vector<Mesh::Vertex>& vertices = mesh->GetVertices();
				std::vector<uint>& indices = mesh->GetIndices();

				//number of indices
				uint noofIndices = (uint)indices.size();

				for (uint j = 0; j < noofIndices; j += 3)
				{
					BVHAABB bbox;

					int index0 = indices[j + 0];
					int index1 = indices[j + 1];
					int index2 = indices[j + 2];

					XMVECTOR pos1 = Float3ToVector4(vertices[index0].position);
					XMVECTOR pos2 = Float3ToVector4(vertices[index1].position);
					XMVECTOR pos3 = Float3ToVector4(vertices[index2].position);

					bbox.Vertex0 = Vector4ToFloat3(XMVector4Transform(pos1, world));
					bbox.Vertex1 = Vector4ToFloat3(XMVector4Transform(pos2, world));
					bbox.Vertex2 = Vector4ToFloat3(XMVector4Transform(pos3, world));

					bbox.Expand(bbox.Vertex0);
					bbox.Expand(bbox.Vertex1);
					bbox.Expand(bbox.Vertex2);

					XMFLOAT3 centre = bbox.GetCentre();

					bbox.InstanceID = 0;

					bboxData.emplace_back(bbox);

					bboxCentres.emplace_back(centre, index);

					XMVECTOR normal1 = XMVector3Normalize( Float3ToVector4(vertices[index0].normal, 0) );
					XMVECTOR normal2 = XMVector3Normalize( Float3ToVector4(vertices[index1].normal, 0) );
					XMVECTOR normal3 = XMVector3Normalize( Float3ToVector4(vertices[index2].normal, 0) );

					m_normalsList.push_back( Vector4ToFloat3(XMVector4Transform(normal1, world)) );
					m_normalsList.push_back( Vector4ToFloat3(XMVector4Transform(normal2, world)) );
					m_normalsList.push_back( Vector4ToFloat3(XMVector4Transform(normal3, world)) );

					m_uvList.push_back( vertices[index0].texcoord );
					m_uvList.push_back( vertices[index1].texcoord );
					m_uvList.push_back( vertices[index2].texcoord );

					m_materialIDList.push_back(materialID);

					index++;
				}
			}
		}
	}

	void CalculateBounds(std::vector<BVHAABB>& bboxData, std::vector<BVHAABBCentre>&	bboxCentres, int begin, int end, XMFLOAT3& minBounds, XMFLOAT3& maxBounds)
	{
		minBounds = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		maxBounds = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for (int i = begin; i <= end; i++)
		{
			const int index = bboxCentres[i].AABBoxIndex;

			//find bounding box
			minBounds = XMFloat3Min(bboxData[index].MinBounds, minBounds);
			maxBounds = XMFloat3Max(bboxData[index].MaxBounds, maxBounds);
		}
	}

	void SortAlongAxis(std::vector<BVHAABB>& bboxData, std::vector<BVHAABBCentre>& bboxCentres, int begin, int end, int axis)
	{
		if (axis == 0)
			std::sort(bboxCentres.begin() + begin, bboxCentres.begin() + end + 1, [](const BVHAABBCentre& arg1, const BVHAABBCentre& arg2) { return (arg1.Centre.x < arg2.Centre.x); });
		else if (axis == 1)
			std::sort(bboxCentres.begin() + begin, bboxCentres.begin() + end + 1, [](const BVHAABBCentre& arg1, const BVHAABBCentre& arg2) { return (arg1.Centre.y < arg2.Centre.y); });
		else
			std::sort(bboxCentres.begin() + begin, bboxCentres.begin() + end + 1, [](const BVHAABBCentre& arg1, const BVHAABBCentre& arg2) { return (arg1.Centre.z < arg2.Centre.z); });
	}

	inline float CalculateSurfaceArea(const AABB& bbox)
	{
		XMFLOAT3 extents = XMFloat3Sub(bbox.MaxBounds, bbox.MinBounds);
		return (extents.x * extents.y + extents.y * extents.z + extents.z * extents.x) * 2.0f;
	}

	struct SurfaceCostElement
	{
		float SurfaceAreaLeft;
		float SurfaceAreaRight;
	};

	std::vector<SurfaceCostElement> SurfaceCost;

	//based on https://github.com/kayru/RayTracedShadows/blob/master/Source/BVHBuilder.cpp
	void FindBestSplit(std::vector<BVHAABB>& bboxData, std::vector<BVHAABBCentre>& bboxCentres, int begin, int end, int& split, int& axis, float& splitCost)
	{
		int count = end - begin + 1;
		int bestSplit = begin;
		int globalBestSplit = begin;
		splitCost = FLT_MAX;

		XMFLOAT3 minBounds;
		XMFLOAT3 maxBounds;
		CalculateBounds(bboxData, bboxCentres, begin, end, minBounds, maxBounds);

		XMFLOAT3 boxSize = XMFloat3Sub(maxBounds, minBounds);

		float maxDim = XMFloat3MaxElement(boxSize);

		axis = 2;
		if (maxDim == boxSize.x)
			axis = 0;
		else if (maxDim == boxSize.y)
			axis = 1;

		split = begin;

		int i = axis;

		//for (int i = 0; i < 3; i++)
		{
			SortAlongAxis(bboxData, bboxCentres, begin, end, i);

			BVHAABB boundsLeft;
			BVHAABB boundsRight;

			for (int indexLeft = 0; indexLeft < count; ++indexLeft)
			{
				const int indexRight = count - indexLeft - 1;

				const int bboxDataIndexLeft = bboxCentres[begin + indexLeft].AABBoxIndex;
				const int bboxDataIndexRight = bboxCentres[begin + indexRight].AABBoxIndex;

				boundsLeft.Expand(bboxData[bboxDataIndexLeft].MinBounds);
				boundsLeft.Expand(bboxData[bboxDataIndexLeft].MaxBounds);

				boundsRight.Expand(bboxData[bboxDataIndexRight].MinBounds);
				boundsRight.Expand(bboxData[bboxDataIndexRight].MaxBounds);

				const float surfaceAreaLeft = CalculateSurfaceArea(boundsLeft);
				const float surfaceAreaRight = CalculateSurfaceArea(boundsRight);

				SurfaceCost[begin + indexLeft].SurfaceAreaLeft = surfaceAreaLeft;
				SurfaceCost[begin + indexRight].SurfaceAreaRight = surfaceAreaRight;
			}

			float bestCost = FLT_MAX;
			for (int mid = begin + 1; mid <= end; ++mid)
			{
				const float surfaceAreaLeft = SurfaceCost[mid - 1].SurfaceAreaLeft;
				const float surfaceAreaRight = SurfaceCost[mid].SurfaceAreaRight;

				const int countLeft = mid - begin;
				const int countRight = end - mid + 1;

				const float costLeft = surfaceAreaLeft * (float)countLeft;
				const float costRight = surfaceAreaRight * (float)countRight;

				const float cost = costLeft + costRight;

				if (cost < bestCost)
				{
					bestSplit = mid;
					bestCost = cost;
				}
			}

			if (bestCost < splitCost)
			{
				split = bestSplit;
				splitCost = bestCost;
				axis = i;
			}
		}
	}

	BVHNode* CreateBVHNodeSHA(std::vector<BVHAABB>& bboxData, std::vector<BVHAABBCentre>&	bboxCentres, int begin, int end, float parentSplitCost)
	{
		int count = end - begin + 1;

		XMFLOAT3 minBounds;
		XMFLOAT3 maxBounds;

		BVHNode* node = new BVHNode();

		if (count == 1)
		{
			//this is a leaf node
			node->Left = nullptr;
			node->Right = nullptr;

			const int index = bboxCentres[begin].AABBoxIndex;

			node->BoundingBox.InstanceID = bboxData[index].InstanceID;

			node->BoundingBox.Vertex0 = bboxData[index].Vertex0;
			node->BoundingBox.Vertex1 = bboxData[index].Vertex1;
			node->BoundingBox.Vertex2 = bboxData[index].Vertex2;

			node->BoundingBox.Expand(node->BoundingBox.Vertex0);
			node->BoundingBox.Expand(node->BoundingBox.Vertex1);
			node->BoundingBox.Expand(node->BoundingBox.Vertex2);

			node->TriangleIndex = index;
		}
		else
		{
			int split;
			int axis;
			float splitCost;

			//find the best axis to sort along and where the split should be according to SAH
			FindBestSplit(bboxData, bboxCentres, begin, end, split, axis, splitCost);

			//sort along that axis
			SortAlongAxis(bboxData, bboxCentres, begin, end, axis);

			CalculateBounds(bboxData, bboxCentres, begin, end, minBounds, maxBounds);

			node->BoundingBox.Expand(minBounds);
			node->BoundingBox.Expand(maxBounds);

			//create the two branches
			node->Left = CreateBVHNodeSHA(bboxData, bboxCentres, begin, split - 1, splitCost);
			node->Right = CreateBVHNodeSHA(bboxData, bboxCentres, split, end, splitCost);

			//Access the child with the largest probability of collision first.
			float surfaceAreaLeft = CalculateSurfaceArea(node->Left->BoundingBox);
			float surfaceAreaRight = CalculateSurfaceArea(node->Right->BoundingBox);

			if (surfaceAreaRight > surfaceAreaLeft)
			{
				BVHNode* temp = node->Right;
				node->Right = node->Left;
				node->Left = temp;
			}

			node->BoundingBox.Vertex0 = XMFLOAT3(0.0f, 0.0f, 0.0f);
			node->BoundingBox.Vertex1 = XMFLOAT3(0.0f, 0.0f, 0.0f);
			node->BoundingBox.Vertex2 = XMFLOAT3(0.0f, 0.0f, 0.0f);

			//this is an intermediate Node
			node->BoundingBox.InstanceID = -1;
		}

		return node;
	}

	void WriteBVHTree(BVHNode *root, BVHNode *node, uint8* bboxData, int& dataOffset, int& index)
	{
		if (node)
		{
			if (node->BoundingBox.InstanceID < 0.0f)
			{
				int tempIndex = 0;
				int tempdataOffset = 0;
				BVHNodeBBoxGPU* bbox = nullptr;

				//do not write the root node, for secondary rays the origin will always be in the scene bounding box
				if (node != root)
				{
					//this is an intermediate node, write bounding box
					bbox = (BVHNodeBBoxGPU*)(bboxData + dataOffset);

					bbox->MinBounds = ToFloat4(node->BoundingBox.MinBounds);
					bbox->MaxBounds = ToFloat4(node->BoundingBox.MaxBounds);

					tempIndex = index;

					dataOffset += sizeof(BVHNodeBBoxGPU);
					index++;

					tempdataOffset = dataOffset;
				}

				WriteBVHTree(root, node->Left, bboxData, dataOffset, index);
				WriteBVHTree(root, node->Right, bboxData, dataOffset, index);

				if (node != root)
				{
					//when on the left branch, how many float4 elements we need to skip to reach the right branch?
					bbox->MinBounds.w = -(float)(dataOffset - tempdataOffset) / sizeof(XMFLOAT4);
				}
			}
			else
			{
				//leaf node, write triangle vertices
				BVHLeafBBoxGPU* bbox = (BVHLeafBBoxGPU*)(bboxData + dataOffset);

				bbox->Vertex0 = ToFloat4(node->BoundingBox.Vertex0);
				bbox->Vertex1MinusVertex0 = ToFloat4(XMFloat3Sub(node->BoundingBox.Vertex1, node->BoundingBox.Vertex0));
				bbox->Vertex2MinusVertex0 = ToFloat4(XMFloat3Sub(node->BoundingBox.Vertex2, node->BoundingBox.Vertex0));

				//when on the left branch, how many float4 elements we need to skip to reach the right branch?
				bbox->Vertex0.w = sizeof(BVHLeafBBoxGPU) / sizeof(XMFLOAT4);
				bbox->Vertex1MinusVertex0.w = node->TriangleIndex;// store the triangle index, we  need it to access normals and uvs
				bbox->Vertex2MinusVertex0.w = m_materialIDList[node->TriangleIndex];

				dataOffset += sizeof(BVHLeafBBoxGPU);
				index++;
			}
		}
	}


	void DeleteBVHTree(BVHNode *node)
	{
		if (node && (node->Left != nullptr || node->Right != nullptr))
		{
			DeleteBVHTree(node->Left);
			DeleteBVHTree(node->Right);
			
			delete node;
		}
	}

	void CreateBVHBuffer(Scene* scene)
	{
		uint noofBoxes = 0;

		for (ModelInstance* modelInstance : scene->GetModelInstances())
		{
			Model* model = modelInstance->GetModel();

			for (Mesh* mesh : model->GetMeshes())
			{
				noofBoxes += mesh->GetNoofIndices() / 3;
			}
		}
	
		std::vector<BVHAABB> triBBoxes;
		triBBoxes.reserve(noofBoxes);

		std::vector<BVHAABBCentre>	bboxCentres;
		bboxCentres.reserve(noofBoxes);

		SurfaceCost = std::vector<SurfaceCostElement>(noofBoxes);

		//create buffers for BVH
		AddPropToPrimitivesAABBList(triBBoxes, bboxCentres, scene);

		int count = (int)triBBoxes.size();
		BVHNode* bvhRoot = CreateBVHNodeSHA(triBBoxes, bboxCentres, 0, count - 1, FLT_MAX);

		const uint maxNoofElements = (uint)(2.5f * count);

		uint8* bvhTreeNodesGPU = (uint8*)malloc(maxNoofElements * sizeof(BVHLeafBBoxGPU));

		int dataOffset = 0;
		int index = 0;
		WriteBVHTree(bvhRoot, bvhRoot, bvhTreeNodesGPU, dataOffset, index);

		//terminate BVH tree
		BVHNodeBBoxGPU* bbox = (BVHNodeBBoxGPU*)(bvhTreeNodesGPU + dataOffset);
		bbox->MinBounds.w = 0;
		dataOffset += sizeof(BVHNodeBBoxGPU);

		Buffer* bvhBuffer = nullptr;
		{
			Buffer::Description desc;
			desc.m_noofElements = dataOffset / sizeof(XMFLOAT4);
			desc.m_elementSize = sizeof(XMFLOAT4);
			desc.m_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			desc.m_descriptorType = Buffer::DescriptorType::SRV;

			bvhBuffer = new Buffer(desc, L"BVHBuffer", bvhTreeNodesGPU);
		}

		Buffer* normalsBuffer = nullptr;
		{
			Buffer::Description desc;
			desc.m_noofElements = m_normalsList.size();
			desc.m_elementSize = sizeof(XMFLOAT3);
			desc.m_format = DXGI_FORMAT_R32G32B32_FLOAT;
			desc.m_descriptorType = Buffer::DescriptorType::SRV;

			normalsBuffer = new Buffer(desc, L"BVHBuffer_normals", (unsigned char*)m_normalsList.data());
		}

		Buffer* uvBuffer = nullptr;
		{
			Buffer::Description desc;
			desc.m_noofElements = m_uvList.size();
			desc.m_elementSize = sizeof(XMFLOAT2);
			desc.m_format = DXGI_FORMAT_R32G32_FLOAT;
			desc.m_descriptorType = Buffer::DescriptorType::SRV;

			uvBuffer = new Buffer(desc, L"BVHBuffer_uvs", (unsigned char*)m_uvList.data());
		}

		scene->SetBVHBuffer(bvhBuffer, normalsBuffer, uvBuffer);

		free(bvhTreeNodesGPU);
		DeleteBVHTree(bvhRoot);
	}

}