
#define BACKFACE_CULLING 0
#include"raytracingCommon.h"

cbuffer cbPerPass : register(b0)
{
	float4x4	projView;
	float4x4	invProjView;
	float4		rtSize;
	float4		lightDir; 
	float4		cameraPos;
}  
  
Texture2D<float>			depthBuffer : register(t0);
Texture2D<float4>			normalBuffer : register(t1);
Buffer<float4>				BVHTree : register(t2);
RWTexture2D<float>			outputRT : register(u0);
 
//Texture2D					diffuseMaps[] : register(t5, space1);
//SamplerState				samplerPoint : register(s6);

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	bool collision = false;
	int offsetToNextNode = 1;
	 
	float depth = depthBuffer[DTid.xy].x;
	float3 normal = normalBuffer[DTid.xy].xyz;

	float NdotL = dot(normal, lightDir.xyz);

	//do not raytrace for sky pixels and for surfaces that point away from the light
	if (depth < 1 && NdotL > 0)
	{
		float2 uv = DTid.xy * rtSize.zw;

		//get world position from depth
		float4 clipPos = float4(2 * uv - 1, depth, 1);
		clipPos.y = -clipPos.y;

		float4 worldPos = mul(invProjView, clipPos);
		worldPos.xyz /= worldPos.w;

		float3 rayDir = lightDir.xyz;
		float3 rayDirInv = rcp(rayDir);

		//offset to avoid selfshadows
		worldPos.xyz += 2 * normal;

		float t = 0;
		float2 bCoord = 0;

		int dataOffset = 0;  
		bool done = false;

		[loop]
		while (offsetToNextNode != 0)
		{
			float4 element0 = BVHTree[dataOffset++].xyzw;

			offsetToNextNode = int(element0.w);

			collision = false;

			if (offsetToNextNode < 0)
			{
				float4 element1 = BVHTree[dataOffset++].xyzw;

				//try collision against this node's bounding box	
				float3 bboxMin = element0.xyz;
				float3 bboxMax = element1.xyz;

				//intermediate node check for intersection with bounding box
				collision = RayIntersectsBox(worldPos.xyz, rayDirInv, bboxMin.xyz, bboxMax.xyz);

				//if there is collision, go to the next node (left) or else skip over the whole branch
				if (!collision)
					dataOffset -= offsetToNextNode;
			}
			else if (offsetToNextNode > 0)
			{				 
				for (int i = 0; i < (int)element0.y; i+=3)
				{
					float4 vertex0 = BVHTree[dataOffset++].xyzw;
					float4 vertex1MinusVertex0 = BVHTree[dataOffset++].xyzw;
					float4 vertex2MinusVertex0 = BVHTree[dataOffset++].xyzw;
					float4	uv1_2 = BVHTree[dataOffset++].xyzw;

					uint materialID = asuint(vertex0.w);

					//check for intersection with triangle
					collision = RayTriangleIntersect(worldPos.xyz, rayDir, vertex0.xyz, vertex1MinusVertex0.xyz, vertex2MinusVertex0.xyz, t, bCoord);

					//if (collision)
					//{
					//	//get the uv coords of each vertex to interpolate
					//	float2 uv0 = float2(vertex1MinusVertex0.w, vertex2MinusVertex0.w);
					//	float2 uv1 = uv1_2.xy;
					//	float2 uv2 = uv1_2.zw;

					//	float2 uv = (1 - bCoord.x - bCoord.y) * uv0 + bCoord.x * uv1 + bCoord.y * uv2;

					//	float alpha = diffuseMaps[NonUniformResourceIndex(materialID & ~(1 << 31))].SampleLevel(samplerPoint, uv, 2).w;

					//	collision = (alpha >= 0.5);

					//	if (collision)
					//	 break;
					//}
				} 

				if (collision)
				{
					break;
				}
			}
		};
	}

	outputRT[DTid.xy] =   1 - float(collision);
}