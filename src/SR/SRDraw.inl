#pragma once

#include "SRUtils.h"

using namespace DirectX;

#define minOf3(a, b, c) (min(min((a),(b)),(c)))
#define maxOf3(a, b, c) (max(max((a),(b)),(c)))

inline UINT findCorner(XMFLOAT3 v) {
	return (v.x >= 0 ? 1 : 0) + (v.y >= 0 ? 2 : 0);
}

inline float isTopLeftEdge(XMFLOAT3& edge) {
	return edge.x > 0 || edge.x == 0 && edge.y > 0 ? -0.0f : 0.0f;
}

inline int XM_CALLCONV isTopLeftMask(FXMVECTOR edgeValue, FXMVECTOR topLeftMask) {
	return _mm_movemask_ps(XMVectorOrInt(XMVectorGreater(edgeValue, XMVectorZero()),
		XMVectorAndInt(XMVectorEqual(edgeValue, XMVectorZero()), topLeftMask)));
}

XMVECTOR indexToSelect1(UINT indices[3]) {
	XMVECTORU32 ans = {
			indices[0] % 2 == 0 ? XM_SELECT_0 : XM_SELECT_1,
			indices[1] % 2 == 0 ? XM_SELECT_0 : XM_SELECT_1,
			indices[2] % 2 == 0 ? XM_SELECT_0 : XM_SELECT_1,
			indices[2] % 2 == 0 ? XM_SELECT_0 : XM_SELECT_1
	};
	return XMVECTOR(ans);
}

XMVECTOR indexToSelect2(UINT indices[3]) {
	XMVECTORU32 ans = {
			indices[0] / 2 == 0 ? XM_SELECT_0 : XM_SELECT_1,
			indices[1] / 2 == 0 ? XM_SELECT_0 : XM_SELECT_1,
			indices[2] / 2 == 0 ? XM_SELECT_0 : XM_SELECT_1,
			indices[2] / 2 == 0 ? XM_SELECT_0 : XM_SELECT_1
	};
	return XMVECTOR(ans);
}

inline void XM_CALLCONV horizontalMinMax(FXMVECTOR value, float& min, float& max) {
	XMVECTOR tmp = XMVectorSwizzle<1, 0, 3, 2>(value);
	XMVECTOR minOfTwo = XMVectorMin(value, tmp);
	XMVECTOR maxOfTwo = XMVectorMax(value, tmp);
	tmp = XMVectorSwizzle<2, 3, 0, 1>(minOfTwo);
	min = XMVectorGetX(XMVectorMin(minOfTwo, tmp));
	tmp = XMVectorSwizzle<2, 3, 0, 1>(maxOfTwo);
	max = XMVectorGetX(XMVectorMax(maxOfTwo, tmp));
}

// [e1max, e2max, e3max, 0]
inline XMVECTOR XM_CALLCONV selectMaxCorner(FXMMATRIX mat, XMVECTOR select1, XMVECTOR select2) {
	// really sad we cannot use _mm_suffle_ps with dynamic mask, it is much more efficient.
	XMVECTOR first2 = XMVectorSelect(mat.r[0], mat.r[1], select1);
	XMVECTOR second2 = XMVectorSelect(mat.r[2], mat.r[3], select1);
	return XMVectorSelect(first2, second2, select2);
}

// [e1min, e2min, e3min, 0]
inline XMVECTOR XM_CALLCONV selectMinCorner(FXMMATRIX mat, XMVECTOR select1, XMVECTOR select2) {
	XMVECTOR first2 = XMVectorSelect(mat.r[3], mat.r[2], select1);
	XMVECTOR second2 = XMVectorSelect(mat.r[1], mat.r[0], select1);
	return XMVectorSelect(first2, second2, select2);
}

inline float IntersectParameter(float a0, float a1) {
	return a0 / (a0 - a1);
}

void InterpolateLine(BYTE* p0, BYTE* p1, float t, int count, BYTE* pOut) {
	// note: alias is possible
	float *p0f = reinterpret_cast<float*>(p0);
	float *p1f = reinterpret_cast<float*>(p1);
	float *pOutf = reinterpret_cast<float*>(pOut);
	for (int i = 0; i < count; i++) {
		pOutf[i] = p0f[i] + t * (p1f[i] - p0f[i]);
	}
}

// return pointer needs to be freed by caller
const BYTE*const* SRDevice::AssempleConstantBuffers() {
	const BYTE** constBuffersTmp = nullptr;
	if (mPipelineState.NumConstantBuffer != 0) {
		constBuffersTmp = (const BYTE**)malloc(mPipelineState.NumConstantBuffer * sizeof(BYTE*));
		assert(constBuffersTmp != nullptr);
		for (UINT i = 0; i < mPipelineState.NumConstantBuffer; i++) {
			assert(mConstantsBufferHandle[i] != InvalidHandle);
			constBuffersTmp[i] = mResources[mConstantsBufferHandle[i]].ptr;
		}
	}
	return constBuffersTmp;
}