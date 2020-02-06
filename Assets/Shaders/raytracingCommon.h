
#define USE_SHADOWS_AA 0

#define kEpsilon 0.00001
#define RAY_OFFSET 0.05

//#define RT_USE_BYTEADDRESS_BUFFER
#define RT_USE_STRUCTURED_BUFFER

#if defined(RT_USE_STRUCTURED_BUFFER)
#define BVHTreeBuffer StructuredBuffer<float4>
#define BVHNormalsBuffer StructuredBuffer<float3>
#define BVHUVsBuffer StructuredBuffer<float2>

float4 BVHTreeLoad4(in BVHTreeBuffer bvhTree, inout int offset)
{
//	float4 element = float4(bvhTree[offset++].x, bvhTree[offset++].x, bvhTree[offset++].x, bvhTree[offset++].x);
	float4 element = bvhTree[offset++].xyzw;

	return element;
}

int BVHTreeOffsetToNextNode(int offset)
{
	return -offset;
//	return -4 * offset;
}
#elif defined(RT_USE_BYTEADDRESS_BUFFER)

#define BVHTreeBuffer ByteAddressBuffer
#define BVHNormalsBuffer StructuredBuffer<float3>
#define BVHUVsBuffer StructuredBuffer<float2>

float4 BVHTreeLoad4(in BVHTreeBuffer bvhTree, inout int offset)
{
	float4 element = asfloat(bvhTree.Load4(offset).xyzw);

	offset += 4*4;

	return element;
}

int BVHTreeOffsetToNextNode(int offset)
{
	return -offset*4*4;
}

#else
#define BVHTreeBuffer Buffer<float4>
#define BVHNormalsBuffer Buffer<float4>
#define BVHUVsBuffer Buffer<float2>
//#define BVHBuffer ByteAddressBuffer 

float4 BVHTreeLoad4(in BVHTreeBuffer bvhTree, inout int offset)
{
	float4 element = bvhTree[offset++].xyzw;
	return element;
}

int BVHTreeOffsetToNextNode(int offset)
{
	return -offset;
}

#endif

bool RayIntersectsBox(float3 origin, float3 rayDirInv, float3 BboxMin, float3 BboxMax) 
{
	const float3 t0 = (BboxMin - origin) * rayDirInv;
	const float3 t1 = (BboxMax - origin) * rayDirInv;

	const float3 tmax = max(t0, t1);
	const float3 tmin = min(t0, t1);

	const float a1 = min(tmax.x, min(tmax.y, tmax.z));
	const float a0 = max( max(tmin.x,tmin.y), max(tmin.z, 0.0f) );

	return a1 >= a0;
}

//Adapted from https://github.com/kayru/RayTracedShadows/blob/master/Source/Shaders/RayTracedShadows.comp
bool RayTriangleIntersect(
	const float3 orig,
	const float3 dir,
	float3 v0,
	float3 e0,
	float3 e1,
	inout float t,
	inout float2 bCoord)
{
	const float3 s1 = cross(dir.xyz, e1);
	const float  invd = 1.0 / (dot(s1, e0));
	const float3 d = orig.xyz - v0;
	bCoord.x = dot(d, s1) * invd;
	const float3 s2 = cross(d, e0);
	bCoord.y = dot(dir.xyz, s2) * invd;
	t = dot(e1, s2) * invd;

	if (
#if BACKFACE_CULLING
		dot(s1, e0) < -kEpsilon ||
#endif
		bCoord.x < 0.0 || bCoord.x > 1.0 || bCoord.y < 0.0 || (bCoord.x + bCoord.y) > 1.0 || t < 0.0 || t > 1e9)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool RayTriangleIntersect(
	const float3 orig,
	const float3 dir,
	float3 v0,
	float3 e0,
	float3 e1)
{
	float t = 0;
	float2 bCoord = 0;
	return RayTriangleIntersect(orig, dir, v0, e0, e1, t, bCoord);
}

struct HitData
{
	int TriangleIndex;
	float2 BarycentricCoords;
	int MaterialID;
	float Distance;
};

bool TraceRay(in BVHTreeBuffer bvhTree, in float3 worldPos, in float3 rayDir)
{
	float t = 0;
	float2 bCoord = 0;

	int dataOffset = 0;
	bool done = false;

	bool collision = false;

	int offsetToNextNode = 1;

	float3 rayDirInv = rcp(rayDir);

	[loop]
	while (offsetToNextNode != 0)
	{
//		float4 element0 = bvhTree[dataOffset++].xyzw;
		float4 element0 = BVHTreeLoad4(bvhTree, dataOffset);

		offsetToNextNode = int(element0.w);

		collision = false;

		if (offsetToNextNode < 0)
		{
//			float4 element1 = bvhTree[dataOffset++].xyzw;		
			float4 element1 = BVHTreeLoad4(bvhTree, dataOffset);

			//try collision against this node's bounding box	
			float3 bboxMin = element0.xyz;
			float3 bboxMax = element1.xyz;

			//intermediate node check for intersection with bounding box
			collision = RayIntersectsBox(worldPos.xyz, rayDirInv, bboxMin.xyz, bboxMax.xyz);

			//if there is collision, go to the next node (left) or else skip over the whole branch
			if (!collision)
				dataOffset += BVHTreeOffsetToNextNode( offsetToNextNode );
		}
		else if (offsetToNextNode > 0)
		{
			float4 vertex0 = element0;
//			float4 vertex1MinusVertex0 = bvhTree[dataOffset++].xyzw;
//			float4 vertex2MinusVertex0 = bvhTree[dataOffset++].xyzw;
			float4 vertex1MinusVertex0 = BVHTreeLoad4(bvhTree, dataOffset);
			float4 vertex2MinusVertex0 = BVHTreeLoad4(bvhTree, dataOffset);

			//check for intersection with triangle
			collision = RayTriangleIntersect(worldPos.xyz, rayDir, vertex0.xyz, vertex1MinusVertex0.xyz, vertex2MinusVertex0.xyz, t, bCoord);
			
			if (collision)
				return true;
		}
	};

	return false;
}

bool TraceRay(in BVHTreeBuffer bvhTree, float3 worldPos, float3 rayDir, bool firstHitOnly, out HitData hitData, float maxDistance = 100000)
{
	float t = 0;
	float2 bCoord = 0;
	float minDistance = 1000000;

	int dataOffset = 0;
	bool done = false;

	bool collision = false;

	int offsetToNextNode = 1;

	float3 rayDirInv = rcp(rayDir);

	int noofCollisions = 0;

	[loop]
	while (offsetToNextNode != 0)
	{
		//float4 element0 = bvhTree[dataOffset++].xyzw;
		float4 element0 = BVHTreeLoad4(bvhTree, dataOffset);

		offsetToNextNode = int(element0.w);

		collision = false;

		if (offsetToNextNode < 0)
		{
			//float4 element1 = bvhTree[dataOffset++].xyzw;
			float4 element1 = BVHTreeLoad4(bvhTree, dataOffset);

			//try collision against this node's bounding box	
			float3 bboxMin = element0.xyz;
			float3 bboxMax = element1.xyz;

			//intermediate node check for intersection with bounding box
			collision = RayIntersectsBox(worldPos.xyz, rayDirInv, bboxMin.xyz, bboxMax.xyz);

			//if there is collision, go to the next node (left) or else skip over the whole branch
			if (!collision)
				dataOffset += BVHTreeOffsetToNextNode(offsetToNextNode);
		}
		else if (offsetToNextNode > 0)
		{
			float4 vertex0 = element0;
			//float4 vertex1MinusVertex0 = bvhTree[dataOffset++].xyzw;
			//float4 vertex2MinusVertex0 = bvhTree[dataOffset++].xyzw;
			float4 vertex1MinusVertex0 = BVHTreeLoad4(bvhTree, dataOffset);
			float4 vertex2MinusVertex0 = BVHTreeLoad4(bvhTree, dataOffset);

			//check for intersection with triangle
			collision = RayTriangleIntersect(worldPos.xyz, rayDir, vertex0.xyz, vertex1MinusVertex0.xyz, vertex2MinusVertex0.xyz, t, bCoord);

			collision = collision && t <= maxDistance;

			if (collision && t < minDistance)
			{
				hitData.TriangleIndex = (int)vertex1MinusVertex0.w;
				hitData.MaterialID = (int)vertex2MinusVertex0.w;
				hitData.BarycentricCoords = bCoord;
				hitData.Distance = t;

				minDistance = t;

				noofCollisions++;

				if (firstHitOnly || noofCollisions > 10)
					return true;
			}

		}
	};

	return noofCollisions > 0;
}
