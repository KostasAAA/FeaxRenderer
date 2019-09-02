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

#define BACKFACE_CULLING 0
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
	float4		CameraPos;

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
	float		pad;

	/*
	float cb_numMips; // the number of mip levels in the convolved color buffer
	float cb_fadeStart; // determines where to start screen edge fading of effect
	float cb_fadeEnd; // determines where to end screen edge fading of effect
	float cb_sslr_padding0; // padding for alignment
	*/
};

Texture2D<float4>	mainBuffer: register(t0);
Texture2D<float>	depthBuffer : register(t1);
Texture2D<float4>	normalBuffer: register(t2);
Buffer<float4>		BVHTree : register(t3);

RWTexture2D<float4>			outputRT : register(u0);

//returns camera space depth
float lineariseDepth(float depth)
{
	// From http://www.humus.name/temp/Linearize%20depth.txt
	// EZ = (n * f) / (f - z * (f - n))

	return (FarPlaneZ * NearPlaneZ) / (FarPlaneZ - depth * (FarPlaneZ - NearPlaneZ));
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
	float depthScale = min(1.0f, z * StrideZCutoff);
	z += ZThickness + lerp(0.0f, 2.0f, depthScale);
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
	float sceneZMax = rayZMax + 100.0f;
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
		// You may need hitPixel.y = depthBufferSize.y - hitPixel.y; here if your vertical axis
		// is different than ours in screen space
		sceneZMax = lineariseDepth(depthBuffer[hitPixel].r);

		PQk += dPQk;
	}

	// Advance Q based on the number of steps
	Q.xy += dQ.xy * stepCount;
	hitPoint = Q * (1.0f / PQk.w);

	return intersectsDepthBuffer(sceneZMax, rayZMin, rayZMax);
}

bool TraceRay(float3 worldPos, float3 rayDir)
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

			if (collision)
				return true;
		}
	};

	return false;
}

#define THREADX 8
#define THREADY 8
#define THREADGROUPSIZE (THREADX*THREADY)

[numthreads(THREADX, THREADY, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint2 screenPos = DTid.xy;

	float depth = depthBuffer[screenPos].r;

	//outputRT[screenPos] = float4(lineariseDepth(depth)/FarPlaneZ,0,0, 0);
	//return;

	if ( depth == 1)
	{
		outputRT[screenPos] = 0;
		return;
	}

	float3 normal = normalBuffer[screenPos].xyz;

	//convert normal to view space to find the correct reflection vector
	float3 normalVS = mul((float3x3)View, normal);

	//get world position from depth
	float2 uv = screenPos.xy * RTSize.zw;
	float4 clipPos = float4(2 * uv - 1, depth, 1);
	clipPos.y = -clipPos.y;

	float4 viewPos = mul(InvProjection, clipPos);
	viewPos.xyz /= viewPos.w;

	float3 rayOrigin = viewPos.xyz;
	float3 toPosition = normalize(rayOrigin.xyz);

	/*
	* Since position is reconstructed in view space, just normalize it to get the
	* vector from the eye to the position and then reflect that around the normal to
	* get the ray direction to trace.
	*/
	float3 rayDirection = reflect(toPosition, normalVS);

	// output rDotV to the alpha channel for use in determining how much to fade the ray
	float rDotV = 1;// dot(rayDirectionVS, toPositionVS);

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

	outputRT[screenPos] = 0;

	if ( intersection)
	{
		outputRT[screenPos] =  mainBuffer[hitPixel];
	}
	else if (false)//raytrace to find reflection
	{
		float4 worldPos = mul(InvView, viewPos);
		toPosition = normalize(worldPos.xyz - CameraPos.xyz);
		rayDirection = reflect(toPosition, normal);

		//offset to avoid self interesection
		worldPos.xyz += 0.01 * normal;

		TraceRay(worldPos.xyz, rayDirection);

		outputRT[screenPos] = 0;
	}

	//return float4(hitPixel, depth, rDotV) * (intersection ? 1.0f : 0.0f);
}

