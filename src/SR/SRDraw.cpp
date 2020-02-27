#include "SRDevice.h"
#include "SRUtils.h"
#include "SRDraw.inl"
#include <omp.h>

//#define AllowQuadPS

using namespace DirectX;

void SRDevice::SRDrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	if (mRenderTargetHandle == InvalidHandle ||
		mDepthStencilHandle == InvalidHandle ||
		mVertexBufferHandle == InvalidHandle)
	{
		SRError(L"Invalid buffer setting.");
		return;
	}

	// only support one instance now.
	assert(InstanceCount == 1);
	// only support intermedia value with float format
	assert(mPipelineState.VSOutputByteCount % 4 == 0);
#ifndef AllowQuadPS
	assert(mPipelineState.EnableQuadPixelShader == false);
#endif
	
	if (mPrimitive == SRPrimitiveTopologyTriangleList) {
		// Input Assembler
		UINT TriangleCount = VertexCountPerInstance / 3;
		BYTE* vsInput = mResources[mVertexBufferHandle].ptr + StartVertexLocation * mPipelineState.VSInputByteStride;

		for (UINT n = 0; n < TriangleCount; n++) {
			const BYTE* vsInputs[3] = {
				vsInput,
				vsInput + mPipelineState.VSInputByteStride,
				vsInput + 2 * mPipelineState.VSInputByteStride
			};
			DrawTriangle(vsInputs);
			vsInput += 3 * mPipelineState.VSInputByteStride;
		}
	}
	else {
		SRError(L"Unsupport Primitive.");
	}
}

void SRDevice::SRDrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
	UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation)
{
	if (mRenderTargetHandle == InvalidHandle ||
		mDepthStencilHandle == InvalidHandle ||
		mVertexBufferHandle == InvalidHandle ||
		mIndexBufferHandle == InvalidHandle)
	{
		SRError(L"Invalid buffer setting.");
		return;
	}

	// only support one instance now.
	assert(InstanceCount == 1);
	// only support intermedia value with float format
	assert(mPipelineState.VSOutputByteCount % 4 == 0);
#ifndef AllowQuadPS
	assert(mPipelineState.EnableQuadPixelShader == false);
#endif

	if (mPrimitive == SRPrimitiveTopologyTriangleList) {
		// Input Assembler
		UINT TriangleCount = IndexCountPerInstance / 3;
		BYTE* vsInput = mResources[mVertexBufferHandle].ptr + BaseVertexLocation * mPipelineState.VSInputByteStride;
		UINT32* indexBuffer = reinterpret_cast<UINT32*>(mResources[mIndexBufferHandle].ptr) + StartIndexLocation;

		for (UINT n = 0; n < TriangleCount; n++) {
			const BYTE* vsInputs[3] = {
				vsInput + mPipelineState.VSInputByteStride * indexBuffer[3 * n],
				vsInput + mPipelineState.VSInputByteStride * indexBuffer[3 * n + 1],
				vsInput + mPipelineState.VSInputByteStride * indexBuffer[3 * n + 2]
			};
			DrawTriangle(vsInputs);
		}
	}
	else {
		SRError(L"Unsupport Primitive.");
	}
}


void SRDevice::DrawTriangle(const BYTE* vsInputs[3]) {
	// pointer setup
	const BYTE*const* constBuffers = AssempleConstantBuffers();

	// since we only do the near plane clip,
	// at most four vertices will be create
	BYTE* vsOutputPool = (BYTE*)malloc(4 * mPipelineState.VSOutputByteCount);
	assert(vsOutputPool != nullptr);
	BYTE* vsOutputs[4] = {
		vsOutputPool,
		vsOutputPool + mPipelineState.VSOutputByteCount,
		vsOutputPool + mPipelineState.VSOutputByteCount * 2,
		vsOutputPool + mPipelineState.VSOutputByteCount * 3
	};


	/****************
	 * Vertex Shader
	 */
	for (UINT i = 0; i < 3; i++) {
		(*mPipelineState.VS)(vsInputs[i], vsOutputs[i], constBuffers);
	}


	/**********************
	 * Near plane clipping
	 */
	float outputZs[3] = {
		*reinterpret_cast<float*>(vsOutputs[0] + 2 * sizeof(float)),
		*reinterpret_cast<float*>(vsOutputs[1] + 2 * sizeof(float)),
		*reinterpret_cast<float*>(vsOutputs[2] + 2 * sizeof(float))
	};

	int numOfOutVertex = 0;
	int outIndex, inIndex;
	for (int i = 0; i < 3; i++) {
		if (outputZs[i] < 0.0f) {
			numOfOutVertex++;
			outIndex = i;
		}
		else
			inIndex = i;
	}
	
	// clip base on the number of vertices out of near plane
	if (numOfOutVertex == 0) {
		RasterizeTriangle(vsOutputs[0], vsOutputs[1], vsOutputs[2], constBuffers);
	}
	else if (numOfOutVertex == 1) {
		int index2 = (outIndex + 1) % 3, index3 = (outIndex + 2) % 3;
		float t0 = IntersectParameter(outputZs[outIndex], outputZs[index2]),
			t1 = IntersectParameter(outputZs[outIndex], outputZs[index3]);
		InterpolateLine(vsOutputs[outIndex], vsOutputs[index2], t0, mPipelineState.VSOutputByteCount / 4, vsOutputs[3]);
		InterpolateLine(vsOutputs[outIndex], vsOutputs[index3], t1, mPipelineState.VSOutputByteCount / 4, vsOutputs[outIndex]);
		RasterizeTriangle(vsOutputs[3], vsOutputs[index2], vsOutputs[index3], constBuffers);
		RasterizeTriangle(vsOutputs[outIndex], vsOutputs[3], vsOutputs[index3], constBuffers);
	}
	else if (numOfOutVertex == 2) {
		int index2 = (inIndex + 1) % 3, index3 = (inIndex + 2) % 3;
		float t0 = IntersectParameter(outputZs[inIndex], outputZs[index2]),
			t1 = IntersectParameter(outputZs[inIndex], outputZs[index3]);
		InterpolateLine(vsOutputs[inIndex], vsOutputs[index2], t0, mPipelineState.VSOutputByteCount / 4, vsOutputs[index2]);
		InterpolateLine(vsOutputs[inIndex], vsOutputs[index3], t1, mPipelineState.VSOutputByteCount / 4, vsOutputs[index3]);
		RasterizeTriangle(vsOutputs[0], vsOutputs[1], vsOutputs[2], constBuffers);
	}

	free(vsOutputPool);
	free(const_cast<BYTE**>(constBuffers));
}


void SRDevice::RasterizeTriangle(const BYTE* vsOutput1, const BYTE* vsOutput2, const BYTE* vsOutput3, const BYTE*const* constBuffers)
{
	// constant setup
	auto& target = mResources[mRenderTargetHandle];
	BYTE* pRenderTarget = target.ptr;
	UINT32* pDepthStencil = reinterpret_cast<UINT32*>(mResources[mDepthStencilHandle].ptr);
	const UINT w = target.WIDTH, h = target.HEIGHT;
	const UINT tileWidth = (w + 7) / 8;
	const bool EnableZPrepass = mPipelineState.EnableZPrePass;
#ifdef AllowQuadPS
	const bool EnableQuadPS = mPipelineState.EnableQuadPixelShader;
#endif

	// pointer setup
#ifdef AllowQuadPS
	BYTE* psInputPool = (BYTE*)malloc(8 * mPipelineState.VSOutputByteCount * 4);
#else
	BYTE* psInputPool = (BYTE*)malloc(8 * mPipelineState.VSOutputByteCount);
#endif
	XMFLOAT3* toInterpolate = (XMFLOAT3*)malloc((mPipelineState.VSOutputByteCount / 4 - 4) * sizeof(XMFLOAT3));
	assert(psInputPool != nullptr);
	assert(toInterpolate != nullptr);

	// openmp thread local pool
	BYTE* psInputs[8];
	for (UINT i = 0; i < 8; i++) {
#ifdef AllowQuadPS
		psInputs[i] = psInputPool + (4 * i) * mPipelineState.VSOutputByteCount;
#else
		psInputs[i] = psInputPool + i * mPipelineState.VSOutputByteCount;
#endif
	}

	/****************
	 * Rasterization
	 * screen mapping and triangle setup
	 */
	const float* vsOutput1f = reinterpret_cast<const float*>(vsOutput1);
	const float* vsOutput2f = reinterpret_cast<const float*>(vsOutput2);
	const float* vsOutput3f = reinterpret_cast<const float*>(vsOutput3);

	XMFLOAT4 p1 = XMFLOAT4(vsOutput1f);
	XMFLOAT4 p2 = XMFLOAT4(vsOutput2f);
	XMFLOAT4 p3 = XMFLOAT4(vsOutput3f);

	// 16.8 fixed-point
	XMStoreFloat4(&p1, XMVectorScale(XMVectorRound(XMVectorScale(XMLoadFloat4(&p1), 256.0f)), 1/256.0f));
	XMStoreFloat4(&p2, XMVectorScale(XMVectorRound(XMVectorScale(XMLoadFloat4(&p2), 256.0f)), 1/256.0f));
	XMStoreFloat4(&p3, XMVectorScale(XMVectorRound(XMVectorScale(XMLoadFloat4(&p3), 256.0f)), 1/256.0f));

	XMFLOAT3 s1 = XMFLOAT3((p1.x / p1.w + 1) * w * 0.5f, (1 - p1.y / p1.w) * h * 0.5f, p1.z / p1.w); //(1.0f - p1.z / p1.w) * 0.5f);
	XMFLOAT3 s2 = XMFLOAT3((p2.x / p2.w + 1) * w * 0.5f, (1 - p2.y / p2.w) * h * 0.5f, p2.z / p2.w); //(1.0f - p2.z / p2.w) * 0.5f);
	XMFLOAT3 s3 = XMFLOAT3((p3.x / p3.w + 1) * w * 0.5f, (1 - p3.y / p3.w) * h * 0.5f, p3.z / p3.w); //(1.0f - p3.z / p3.w) * 0.5f);

	XMFLOAT3 edge1 = XMFLOAT3(s3.y - s2.y, s2.x - s3.x, s3.x * s2.y - s2.x * s3.y);
	XMFLOAT3 edge2 = XMFLOAT3(s1.y - s3.y, s3.x - s1.x, s1.x * s3.y - s3.x * s1.y);
	XMFLOAT3 edge3 = XMFLOAT3(s2.y - s1.y, s1.x - s2.x, s2.x * s1.y - s1.x * s2.y);

	XMVECTOR area = XMVectorReplicate(edge1.z + edge2.z + edge3.z);

	// clockwise culling
	if (XMVectorGetX(area) <= 0.0f)
		goto end;

	XMMATRIX edgeMatrix = {
		XMVectorDivide(XMLoadFloat3(&edge1), area),
		XMVectorDivide(XMLoadFloat3(&edge2), area),
		XMVectorDivide(XMLoadFloat3(&edge3), area),
		g_XMZero
	};

	UINT testCorners[3] = {
		findCorner(edge1),
		findCorner(edge2),
		findCorner(edge3)
	};
	XMVECTOR cornerSelect1 = indexToSelect1(testCorners);
	XMVECTOR cornerSelect2 = indexToSelect2(testCorners);

	// helper variable
	XMVECTOR reci_pW = XMVectorReciprocal(XMVectorSet(p1.w, p2.w, p3.w, INFINITY));
	XMVECTOR sZ = XMVectorSet(s1.z, s2.z, s3.z, 0.0f);
	XMVECTOR topLeftMask = XMVectorSet(isTopLeftEdge(edge1), isTopLeftEdge(edge2), isTopLeftEdge(edge3), -0.0f);

	XMVECTOR edgeA = _mm_shuffle_ps(edgeMatrix.r[0], edgeMatrix.r[1], _MM_SHUFFLE(1, 0, 1, 0)); // [a1 b1 a2 b2]
	XMVECTOR edgeB = _mm_shuffle_ps(edgeA, edgeMatrix.r[2], _MM_SHUFFLE(3, 1, 3, 1)); // [b1 b2 b3 0]
	edgeA = _mm_shuffle_ps(edgeA, edgeMatrix.r[2], _MM_SHUFFLE(3, 0, 2, 0)); // [a1 a2 a3 0]


	// value to be interpolated
	for (UINT i = 0; i < mPipelineState.VSOutputByteCount / 4 - 4; i++) {
		toInterpolate[i] = XMFLOAT3(
			vsOutput1f[i + 4],
			vsOutput2f[i + 4],
			vsOutput3f[i + 4]);
	}


	/*********************
	 * triangle travelsal
	 */
	// 8 * 8 tile
	// axis-aligned bounding box binning
	UINT leftMost = max(UINT(minOf3(s1.x, s2.x, s3.x) / 8), 0);
	UINT rightMost = min(UINT(maxOf3(s1.x, s2.x, s3.x) / 8), (w - 1) / 8);
	UINT topMost = max(UINT(minOf3(s1.y, s2.y, s3.y) / 8), 0);
	UINT bottomMost = min(UINT(maxOf3(s1.y, s2.y, s3.y) / 8), (h - 1) / 8);
//	for (UINT j = topMost; j <= bottomMost; j++) {
//		for (UINT i = leftMost; i <= rightMost; i++) {
#pragma omp parallel for num_threads(8) schedule(dynamic, 8)
	for (int id = 0; id < int((bottomMost - topMost + 1) * (rightMost - leftMost + 1)); id++) {
			UINT i = id % (rightMost - leftMost + 1) + leftMost;
			UINT j = id / (rightMost - leftMost + 1) + topMost;
			// zigzag
			i = (j % 2 == 0 ? i : rightMost + leftMost - i);
			UINT tileXInt = 8 * i;
			UINT tileYInt = 8 * j;
			float tileX = float(tileXInt) + 0.5f;
			float tileY = float(tileYInt) + 0.5f;

			/*
			 * 0--1
			 * |  |
			 * 2--3
			 */
			XMMATRIX cornersT = {
				XMVectorSet(tileX, tileX + 7.0f, tileX, tileX + 7.0f),
				XMVectorSet(tileY, tileY, tileY + 7.0f, tileY + 7.0f),
				g_XMOne,
				g_XMZero
			};

			// tile level edge test
			XMMATRIX edgeXcorners = XMMatrixMultiply(edgeMatrix, cornersT); // m_ij = j-th point's i-th edge value
			XMMATRIX edgeXcornersT = XMMatrixTranspose(edgeXcorners);

			XMVECTOR edgeMax = selectMaxCorner(edgeXcornersT, cornerSelect1, cornerSelect2);
			if (isTopLeftMask(edgeMax, topLeftMask) != 0xf)
				continue;

			XMVECTOR edgeMin = selectMinCorner(edgeXcornersT, cornerSelect1, cornerSelect2);
			
			bool IsAllPixelsValid = isTopLeftMask(edgeMin, topLeftMask) == 0xf;


			// tile size depth test
			XMVECTOR cornerDepths = XMVector3Transform(sZ, edgeXcorners);
			float minOfFour, maxOfFour;
			horizontalMinMax(cornerDepths, minOfFour, maxOfFour);

			UINT32 *pTileHiZ = mInternalHiZCache + (j * tileWidth + i) * 2;
			UINT32 TileHiZMin = *pTileHiZ;
			UINT32 TileHiZMax = *(pTileHiZ + 1);
			float TileHiZMinF = depth2Float(TileHiZMin);
			float TileHiZMaxF = depth2Float(TileHiZMax);
			// I do not take the minimum z of 3 vertices in to consider.
			// Since in my implementation, it would not be helpful too often.
			if (minOfFour >= TileHiZMaxF || maxOfFour < 0.0f)
				continue;

			bool IsAllDepthPass = maxOfFour < TileHiZMinF;

			if (IsAllPixelsValid) {
				assert(minOfFour >= 0.0f);
				TileHiZMin = min(float2Depth(minOfFour), TileHiZMin);
			
				if (IsAllDepthPass)
					TileHiZMax = float2Depth(maxOfFour);
			}

			XMVECTOR edgeCorner0 = edgeXcornersT.r[0];

			bool IsMaxDepthChange = false;

			for (int iy = 0; iy < 4; iy++) {
				for (int t = 0; t < 4; t++) {
					// zigzag
					int ix = iy % 2 == 0 ? t : 3 - t;

#ifdef AllowQuadPS
					if (EnableQuadPS) {
						float* inputs[4];
						inputs[0] = reinterpret_cast<float*>(psInputs[omp_get_thread_num()]);
						inputs[1] = inputs[0] + mPipelineState.VSOutputByteCount / 4;
						inputs[2] = inputs[0] + mPipelineState.VSOutputByteCount / 4 * 2;
						inputs[3] = inputs[0] + mPipelineState.VSOutputByteCount / 4 * 3;
						
						UINT32 depths[4], newDepths[4];
						bool pixelMask[4] = { true, true, true, true };

						// 2 * 2 quad
						for (int u = 0; u < 2; u++) {
							for (int v = 0; v < 2; v++) {
								UINT pxInt = tileXInt + 2 * ix + u;
								UINT pyInt = tileYInt + 2 * iy + v;
								UINT pos = w * pyInt + pxInt;
								int pixelId = 2 * u + v;

								// out of screen test
								if (pxInt >= w || pyInt >= h)
									pixelMask[pixelId] = false;

								// suffix C means coordinate base on upper-left corner
								float pxC = 2.0f * ix + u, pyC = 2.0f * iy + v;

								// used the linear property of edge equation.
								// ks.w = 0.0f
								XMVECTOR ks = XMVectorAdd(edgeCorner0,
									XMVectorAdd(XMVectorScale(edgeA, pxC), XMVectorScale(edgeB, pyC)));

								// pixel level edge test
								if (!IsAllPixelsValid) {
									if (isTopLeftMask(ks, topLeftMask) != 0xf)
										pixelMask[pixelId] = false;
								}

								// homogenes berycentric coordinate
								XMVECTOR k = XMVectorMultiply(ks, reci_pW); // k.w = 0.0f
								XMVECTOR ksDiv_pWSum = XMVectorSum(k);
								k = XMVectorDivide(k, ksDiv_pWSum);

								// SV_POSITION
								float* input = inputs[pixelId];
								input[0] = tileX + pxC;
								input[1] = tileY + pyC;
								input[2] = XMVectorGetX(XMVectorSum(XMVectorMultiply(ks, sZ)));
								input[3] = XMVectorGetX(XMVectorReciprocal(ksDiv_pWSum));

								if (input[2] > 1.0f || input[2] < 0.0f)
									pixelMask[pixelId] = false;
								depths[pixelId] = *(pDepthStencil + pos) >> 8;
								newDepths[pixelId] = float2Depth(input[2]);


								// homogenes linear interploate
								for (UINT i = 0; i < mPipelineState.VSOutputByteCount / 4 - 4; i++) {
									input[i + 4] = XMVectorGetX(
										XMVectorSum(XMVectorMultiply(k, XMLoadFloat3(&toInterpolate[i]))));
								}
							}
						}
						
						// Z-prepass
						if (EnableZPrepass)
							for (int i = 0; i < 4; i++)
								if (newDepths[i] >= depths[i])
									pixelMask[i] = false;

						if (pixelMask[0] == false && pixelMask[1] == false && pixelMask[2] == false && pixelMask[3] == false)
							continue;

						/***************
						 * quad pixel shader
						 */
						XMFLOAT4 pixels[4];
						(*mPipelineState.QuadPS)(reinterpret_cast<BYTE**>(inputs), &pixels, constBuffers);

						/****************
						 * Output Merger
						 */
						for (int i = 0; i < 4; i++) {
							if (!EnableZPrepass) {
								if (inputs[i][2] > 1.0f || inputs[i][2] < 0.0f)
									pixelMask[i] = false;
								newDepths[i] = float2Depth(inputs[i][2]);
							}
							if (pixelMask[i] == false)
								continue;
							if (EnableZPrepass || IsAllDepthPass || newDepths[i] < depths[i]) {
								UINT pxInt = tileXInt + 2 * ix + i / 2;
								UINT pyInt = tileYInt + 2 * iy + i % 2;
								UINT pos = w * pyInt + pxInt;
								BYTE* imagePos = target.ptr + pos * 4;
								imagePos[0] = BYTE(clamp(pixels[i].x) * 255);
								imagePos[1] = BYTE(clamp(pixels[i].y) * 255);
								imagePos[2] = BYTE(clamp(pixels[i].z) * 255);
								imagePos[3] = BYTE(clamp(pixels[i].w) * 255);

								*(pDepthStencil + pos) = (newDepths[i] << 8) | (*(pDepthStencil + pos) & 0xff);

								TileHiZMin = min(newDepths[i], TileHiZMin);

								if (depths[i] == TileHiZMax)
									IsMaxDepthChange = true;
							}
						}
					}
					else {
#endif
					// 2 * 2 quad
					for (int u = 0; u < 2; u++) {
						for (int v = 0; v < 2; v++) {
							UINT pxInt = tileXInt + 2 * ix + u;
							UINT pyInt = tileYInt + 2 * iy + v;
							UINT pos = w * pyInt + pxInt;

							// out of screen test
							if (pxInt >= w || pyInt >= h)
								continue;

							// suffix C means coordinate base on upper-left corner
							float pxC = 2.0f * ix + u, pyC = 2.0f * iy + v;

							// used the linear property of edge equation.
							// ks.w = 0.0f
							XMVECTOR ks = XMVectorAdd(edgeCorner0,
								XMVectorAdd(XMVectorScale(edgeA, pxC), XMVectorScale(edgeB, pyC)));

							// pixel level edge test
							if (!IsAllPixelsValid) {
								if (isTopLeftMask(ks, topLeftMask) != 0xf)
									continue;
							}

							// homogenes berycentric coordinate
							XMVECTOR k = XMVectorMultiply(ks, reci_pW); // k.w = 0.0f
							XMVECTOR ksDiv_pWSum = XMVectorSum(k);
							k = XMVectorDivide(k, ksDiv_pWSum);

							// SV_POSITION
							float* input = reinterpret_cast<float*>(psInputs[omp_get_thread_num()]);
							input[0] = tileX + pxC;
							input[1] = tileY + pyC;
							input[2] = XMVectorGetX(XMVectorSum(XMVectorMultiply(ks, sZ)));
							input[3] = XMVectorGetX(XMVectorReciprocal(ksDiv_pWSum));

							if (input[2] > 1.0f || input[2] < 0.0f)
								continue;
							// Z-prepass
							UINT32 depth = *(pDepthStencil + pos) >> 8;
							UINT32 newDepth = float2Depth(input[2]);
							if (EnableZPrepass && newDepth >= depth)
								continue;

							// homogenes linear interploate
							for (UINT i = 0; i < mPipelineState.VSOutputByteCount / 4 - 4; i++) {
								input[i + 4] = XMVectorGetX(
									XMVectorSum(XMVectorMultiply(k, XMLoadFloat3(&toInterpolate[i]))));
							}


							/***************
								* pixel shader
								*/
							XMFLOAT4 pixel;
							(*mPipelineState.PS)(reinterpret_cast<BYTE*>(input), &pixel, constBuffers);

							/****************
								* Output Merger
								*/
							if (!EnableZPrepass) {
								if (input[2] > 1.0f || input[2] < 0.0f)
									continue;
								newDepth = float2Depth(input[2]);
							}
							if (EnableZPrepass || IsAllDepthPass || newDepth < depth) {
								BYTE* imagePos = target.ptr + pos * 4;
								imagePos[0] = BYTE(clamp(pixel.x) * 255);
								imagePos[1] = BYTE(clamp(pixel.y) * 255);
								imagePos[2] = BYTE(clamp(pixel.z) * 255);
								imagePos[3] = BYTE(clamp(pixel.w) * 255);

								*(pDepthStencil + pos) = (newDepth << 8) | (*(pDepthStencil + pos) & 0xff);

								TileHiZMin = min(newDepth, TileHiZMin);

								if (depth == TileHiZMax)
									IsMaxDepthChange = true;
							}
						}
					}
#ifdef AllowQuadPS
					}
#endif
				}
			}
			if (IsMaxDepthChange && !(IsAllPixelsValid && IsAllDepthPass)) {
				UINT32 max = TileHiZMin;
				for (int j = 0; j < 8; j++) {
					for (int i = 0; i < 8; i++) {
						UINT px = tileXInt + i;
						UINT py = tileYInt + j;
						if (px >= w || py >= h)
							continue;
						UINT pos = w * py+ px;
						max = max(*(pDepthStencil + pos) >> 8, max);
					}
				}
				TileHiZMax = max;
			}
			*pTileHiZ = TileHiZMin;
			*(pTileHiZ + 1) = TileHiZMax;
		}
	//}
end:
	free(psInputPool);
	free(toInterpolate);
}
