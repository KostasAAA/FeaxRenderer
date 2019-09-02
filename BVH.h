#pragma once

using namespace std;
using namespace DirectX;
class Buffer;
class Scene;

namespace BVH
{

	struct BVHAABB : AABB
	{
		XMFLOAT3	Vertex0;
		XMFLOAT3	Vertex1;
		XMFLOAT3	Vertex2;
		int			InstanceID;

		BVHAABB() : AABB()
		{
			InstanceID = 0;
		}
	};


	struct BVHAABBCentre
	{
		XMFLOAT3	Centre;
		int			AABBoxIndex;

		BVHAABBCentre() {}
		BVHAABBCentre(const XMFLOAT3& centre, int index) : AABBoxIndex(index), Centre(centre) {}
	};

	struct BVHNodeBBoxGPU
	{
		XMFLOAT4	MinBounds; // OffsetToNextNode in w component
		XMFLOAT4	MaxBounds;
	};

	struct BVHLeafBBoxGPU
	{
		XMFLOAT4		Vertex0; // OffsetToNextNode in w component
		XMFLOAT4		Vertex1MinusVertex0;
		XMFLOAT4		Vertex2MinusVertex0;
	};

	struct BVHNode
	{
		BVHAABB				BoundingBox;
		BVHNode*			Left;
		BVHNode*			Right;
		float				SplitCost;
		uint				TriangleIndex;
	};
	void CreateBVHBuffer(Scene* scene);

};
