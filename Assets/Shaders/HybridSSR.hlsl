// By Morgan McGuire and Michael Mara at Williams College 2014
// Released as open source under the BSD 2-Clause License
// http://opensource.org/licenses/BSD-2-Clause
//
// Copyright (c) 2014, Morgan McGuire and Michael Mara
// All rights reserved.
//
// From McGuire and Mara, Efficient GPU Screen-Space Ray Tracing,
// Journal of Computer Graphics Techniques, 2014
//
// This software is open source under the "BSD 2-clause license":
//
// Redistribution and use in source and binary forms, with or
// without modification, are permitted provided that the following
// conditions are met:
//
// 1. Redistributions of source code must retain the above
// copyright notice, this list of conditions and the following
// disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
/**
* The ray tracing step of the SSLR implementation.
* Modified version of the work stated above.
*/

#include "LightingCommon.h"

#define BACKFACE_CULLING 1
#include"raytracingCommon.h"

cbuffer cbSSLR : register(b0)
{
	float4x4	ViewProjection;
	float4x4	InvViewProjection;
	float4x4	Projection;
	float4x4	InvProjection;
	float4x4	View;
	float4x4	InvView;
	float4		RTSize;
	float3		CameraPos;
	float		SSRScale;
	float4		LightDirection;

	float		ZThickness; // thickness to ascribe to each pixel in the depth buffer
	float		NearPlaneZ; // the camera's near z plane
	float		FarPlaneZ;
	float		Stride; // Step in horizontal or vertical pixels between samples. This is a float
						// because integer math is slow on GPUs, but should be set to an integer >= 1.
	float		MaxSteps; // Maximum number of iterations. Higher gives better images but may be slow.
	float		MaxDistance; // Maximum camera-space distance to trace before returning a miss.
	float		StrideZCutoff;	// More distant pixels are smaller in screen space. This value tells at what point to
								// start relaxing the stride to give higher quality reflections for objects far from
								// the camera.
	float		ReflectionsMode;

	/*
	float cb_numMips; // the number of mip levels in the convolved color buffer
	float cb_fadeStart; // determines where to start screen edge fading of effect
	float cb_fadeEnd; // determines where to end screen edge fading of effect
	float cb_sslr_padding0; // padding for alignment
	*/
};

struct Material
{
	int			AlbedoID;
	int			NormalID;
	float		Roughness;
	float		Metalness;
};

Texture2D<float4>	mainBuffer: register(t0);
Texture2D<float>	depthBuffer : register(t1);
Texture2D<float4>	normalBuffer: register(t2);
Texture2D<float4>	albedoBuffer: register(t3);
Buffer<float4>		BVHTree : register(t4);
Buffer<float4>		BVHNormals : register(t5);
Buffer<float2>		BVHUVs : register(t6);
StructuredBuffer<Material> Materials :  register(t7);
Texture2D<float4>	Diffuse[] : register(t8);

RWTexture2D<float4>			outputRT : register(u0);

SamplerState SamplerLinear : register(s0);

//returns camera space depth
float lineariseDepth2(float depth)
{
	// From http://www.humus.name/temp/Linearize%20depth.txt
	// EZ = (n * f) / (f - z * (f - n))

	return (FarPlaneZ * NearPlaneZ) / (FarPlaneZ - depth * (FarPlaneZ - NearPlaneZ));
}

float lineariseDepth(float depth)
{
	float ProjectionA = FarPlaneZ / (FarPlaneZ - NearPlaneZ);
	float ProjectionB = (-FarPlaneZ * NearPlaneZ) / (FarPlaneZ - NearPlaneZ);

	// Sample the depth and convert to linear view space Z (assume it gets sampled as
	// a floating point value of the range [0,1])
	float linearDepth = ProjectionB / (depth - ProjectionA);

	return linearDepth;
}

float distanceSquared(float2 a, float2 b)
{
	a -= b;
	return dot(a, a);
}

bool intersectsDepthBuffer(float z, float minZ, float maxZ)
{
	/*
	* Based on how far away from the camera the depth is,
	* adding a bit of extra thickness can help improve some
	* artifacts. Driving this value up too high can cause
	* artifacts of its own.
	*/
	//float depthScale = min(1.0f, z * StrideZCutoff);
	//z += ZThickness + lerp(0.0f, 2.0f, depthScale);
	return (maxZ >= z) && (minZ - ZThickness <= z);
}

void swap(inout float a, inout float b)
{
	float t = a;
	a = b;
	b = t;
}

float linearDepthTexelFetch(int2 hitPixel)
{
	// Load returns 0 for any value accessed out of bounds
	return lineariseDepth(depthBuffer.Load(int3(hitPixel, 0)).r);
}

// Returns true if the ray hit something
bool traceScreenSpaceRay(
	// Camera-space ray origin, which must be within the view volume
	float3 csOrig,
	// Unit length camera-space ray direction
	float3 csDir,
	// Number between 0 and 1 for how far to bump the ray in stride units
	// to conceal banding artifacts. Not needed if stride == 1.
	float jitter,
	// Pixel coordinates of the first intersection with the scene
	out float2 hitPixel,
	// Camera space location of the ray hit
	out float3 hitPoint)
{
	// Clip to the near plane
	float rayLength = ((csOrig.z + csDir.z * MaxDistance) < NearPlaneZ) ?
		(NearPlaneZ - csOrig.z) / csDir.z : MaxDistance;
	float3 csEndPoint = csOrig + csDir * rayLength;

	// Project into homogeneous clip space
	float4 H0 = mul(Projection, float4(csOrig, 1.0f));
	float4 H1 = mul(Projection, float4(csEndPoint, 1.0f));

	float k0 = 1.0f / H0.w;
	float k1 = 1.0f / H1.w;

	// The interpolated homogeneous version of the camera-space points
	float3 Q0 = csOrig * k0;
	float3 Q1 = csEndPoint * k1;

	// Screen-space endpoints
	float2 P0 = H0.xy * k0;
	float2 P1 = H1.xy * k1;

	P0 = P0 * float2(0.5, -0.5) + float2(0.5, 0.5);
	P1 = P1 * float2(0.5, -0.5) + float2(0.5, 0.5);

	P0.xy *= RTSize.xy;
	P1.xy *= RTSize.xy;

	// If the line is degenerate, make it cover at least one pixel
	// to avoid handling zero-pixel extent as a special case later
	P1 += (distanceSquared(P0, P1) < 0.0001f) ? float2(0.01f, 0.01f) : 0.0f;
	float2 delta = P1 - P0;

	// Permute so that the primary iteration is in x to collapse
	// all quadrant-specific DDA cases later
	bool permute = false;
	if (abs(delta.x) < abs(delta.y))
	{
		// This is a more-vertical line
		permute = true;
		delta = delta.yx;
		P0 = P0.yx;
		P1 = P1.yx;
	}

	float stepDir = sign(delta.x);
	float invdx = stepDir / delta.x;

	// Track the derivatives of Q and k
	float3 dQ = (Q1 - Q0) * invdx;
	float dk = (k1 - k0) * invdx;
	float2 dP = float2(stepDir, delta.y * invdx);

	// Scale derivatives by the desired pixel stride and then
	// offset the starting values by the jitter fraction
	float strideScale = 1.0f - min(1.0f, csOrig.z * StrideZCutoff);
	float stride = 1.0f + strideScale * Stride;
	dP *= stride;
	dQ *= stride;
	dk *= stride;

	P0 += dP * jitter;
	Q0 += dQ * jitter;
	k0 += dk * jitter;

	// Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
	float4 PQk = float4(P0, Q0.z, k0);
	float4 dPQk = float4(dP, dQ.z, dk);
	float3 Q = Q0;

	// Adjust end condition for iteration direction
	float end = P1.x * stepDir;

	float stepCount = 0.0f;
	float prevZMaxEstimate = csOrig.z;
	float rayZMin = prevZMaxEstimate;
	float rayZMax = prevZMaxEstimate;
	float sceneZMax = rayZMax + 200.0f;

	for (;
		((PQk.x * stepDir) <= end) && (stepCount < MaxSteps) &&
		!intersectsDepthBuffer(sceneZMax, rayZMin, rayZMax) &&
		(sceneZMax != 0.0f);
		++stepCount)
	{
		rayZMin = prevZMaxEstimate;
		rayZMax = (dPQk.z * 0.5f + PQk.z) / (dPQk.w * 0.5f + PQk.w);
		prevZMaxEstimate = rayZMax;

		if (rayZMin > rayZMax)
		{
		 	swap(rayZMin, rayZMax);
		}

		hitPixel = permute ? PQk.yx : PQk.xy;

		sceneZMax = lineariseDepth(depthBuffer[hitPixel].r);

		PQk += dPQk;
	}

	// Advance Q based on the number of steps
	Q.xy += dQ.xy * stepCount;
	hitPoint = Q * (1.0f / PQk.w);

	return intersectsDepthBuffer(sceneZMax, rayZMin, rayZMax);
}

struct HitData
{
	int TriangleIndex;
	float2 BarycentricCoords;
	int MaterialID;
	float Distance;
};

bool TraceRay(float3 worldPos, float3 rayDir, bool firstHitOnly, out HitData hitData)
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
			float4 vertex0 = element0;
			float4 vertex1MinusVertex0 = BVHTree[dataOffset++].xyzw;
			float4 vertex2MinusVertex0 = BVHTree[dataOffset++].xyzw;

			//check for intersection with triangle
			collision = RayTriangleIntersect(worldPos.xyz, rayDir, vertex0.xyz, vertex1MinusVertex0.xyz, vertex2MinusVertex0.xyz, t, bCoord);

			if (collision && t < minDistance)
			{
				hitData.TriangleIndex = (int)vertex1MinusVertex0.w;
				hitData.MaterialID = (int)vertex2MinusVertex0.w;
				hitData.BarycentricCoords = bCoord;
				hitData.Distance = t;

				minDistance = t;

				noofCollisions++;

				if(firstHitOnly || noofCollisions > 10)
					return true;
			}
		}
	};

	return noofCollisions > 0;
}

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint2 screenPos = DTid.xy;

	if (ReflectionsMode == 1)
	{
		outputRT[screenPos] = 0;
		return;
	}

	float depth = depthBuffer[screenPos].r;

	if ( depth == 1)
	{
		outputRT[screenPos] = 0;
		return;
	}

	float4 albedo = albedoBuffer[screenPos];
	float4 normal = normalBuffer[screenPos].xyzw;

	//convert normal to view space to find the correct reflection vector
	float3 normalVS = mul((float3x3)View, normal.xyz);

	//get world position from depth
	float2 uv = screenPos.xy * RTSize.zw;
	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float4 viewPos = mul(InvProjection, clipPos);
	viewPos.xyz /= viewPos.w;

	float3 rayOrigin = viewPos.xyz + normalVS * 0.01;
	float3 toPosition = normalize(rayOrigin.xyz);

	/*
	* Since position is reconstructed in view space, just normalize it to get the
	* vector from the eye to the position and then reflect that around the normal to
	* get the ray direction to trace.
	*/
	float3 rayDirection = reflect(toPosition, normalVS);

	// out parameters
	float2 hitPixel = float2(0.0f, 0.0f);
	float3 hitPoint = float3(0.0f, 0.0f, 0.0f);

	float jitter = Stride > 1.0f ? float(int(screenPos.x + screenPos.y) & 1) * 0.5f : 0.0f;

	// perform ray tracing - true if hit found, false otherwise
	bool intersection = traceScreenSpaceRay(rayOrigin, rayDirection, jitter, hitPixel, hitPoint);

	depth = depthBuffer.Load(int3(hitPixel, 0)).r;

	// move hit pixel from pixel position to UVs;
	if (hitPixel.x > RTSize.x || hitPixel.x < 0.0f || hitPixel.y > RTSize.y || hitPixel.y < 0.0f)
	{
		intersection = false;
	}

	float4 result = 0;// float4(0.0f, 0.5f, 1.0f, 1);

	float4 worldPos = mul(InvViewProjection, clipPos);
	worldPos.xyz /= worldPos.w;

	toPosition = normalize(worldPos.xyz - CameraPos.xyz);
	rayDirection = reflect(toPosition, normal.xyz);

	if ( intersection && !((int)ReflectionsMode & 1) )
	{
		result =  mainBuffer[hitPixel];
	}
	else if ( ReflectionsMode >= 3.0 ) //raytrace to find reflection
	{
		//offset to avoid self interesection
		worldPos.xyz += 0.05 * normal.xyz;

		HitData hitdata;

		if (TraceRay(worldPos.xyz, rayDirection, false, hitdata))
		{
			//interpolate normal
			float3 n0 = BVHNormals[hitdata.TriangleIndex * 3].xyz;
			float3 n1 = BVHNormals[hitdata.TriangleIndex * 3 + 1].xyz;
			float3 n2 = BVHNormals[hitdata.TriangleIndex * 3 + 2].xyz;

			float3 n = n0 * (1 - hitdata.BarycentricCoords.x - hitdata.BarycentricCoords.y) + n1 * hitdata.BarycentricCoords.x + n2 * hitdata.BarycentricCoords.y;
			n = normalize(n);

			//interpolate uvs
			float2 uv0 = BVHUVs[hitdata.TriangleIndex * 3].xy;
			float2 uv1 = BVHUVs[hitdata.TriangleIndex * 3 + 1].xy;
			float2 uv2 = BVHUVs[hitdata.TriangleIndex * 3 + 2].xy;

			float2 uvCoord = uv0 * (1 - hitdata.BarycentricCoords.x - hitdata.BarycentricCoords.y) + uv1 * hitdata.BarycentricCoords.x + uv2 * hitdata.BarycentricCoords.y;

			//get material data for hit point
			Material material = Materials[NonUniformResourceIndex(hitdata.MaterialID)];

			//sample texture of hit point
			float3 albedo = Diffuse[NonUniformResourceIndex(material.AlbedoID)].SampleLevel(SamplerLinear, uvCoord, 0).rgb;
			albedo.rgb = pow(albedo.rgb, 2.2);

			//float3 colours[4] = { float3(1,0,0), float3(0,1,0), float3(0,0,1), float3(1,1,0) };
			//outputRT[screenPos] = float4(colours[hitdata.MaterialID], 1);

			float3 specularColour = lerp(0.04, albedo.rgb, material.Metalness);
			albedo.rgb = lerp(albedo.rgb, 0, material.Metalness);

			//calculate specular for hit point. This assumes view dir is hitpoint to world position (-rayDirection)
			float3 specular = LightingGGX(n.xyz, -rayDirection, LightDirection.xyz, material.Roughness, specularColour);

			worldPos.xyz += hitdata.Distance * rayDirection + 0.1 * n; 

			bool collision = TraceRay(worldPos.xyz, LightDirection.xyz, true, hitdata);

			float NdotL = saturate(dot(n, LightDirection.xyz));

			float lightIntensity = LightDirection.w * (1 - collision);

			result =  float4( albedo.rgb * (lightIntensity * NdotL + 0.3) + lightIntensity * specular, 1);
		}
	}

	//calculate fresnel for the world point/pixel we are shading
	float3 L = rayDirection.xyz;
	float3 H = normalize(-toPosition + L);

	float3 specularColour = lerp(0.04, albedo.rgb, albedo.a);

	float3 F = Fresnel(specularColour, L, H);

	outputRT[screenPos] = float4( result.rgb * (F * SSRScale) , 1);
}

