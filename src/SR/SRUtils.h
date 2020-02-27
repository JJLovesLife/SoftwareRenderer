#pragma once

#include <dxgi1_4.h>
#include <math.h>

#define SRError(x) if (mDebugLayer) MessageBox(mhMainWnd, (x), L"SR Error", 0)
#define SRFatal(x) MessageBox(mhMainWnd, (x), L"SR Fatal", 0)

#define DepthMax ((1 << 24) - 1)

extern constexpr int SizeOfFormat(DXGI_FORMAT format);

inline float clamp(float x) {
	return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

inline UINT32 float2Depth(float depth) {
	return UINT32(depth * DepthMax);
}

inline UINT32 float2DpethCeil(float depth) {
	return UINT32(ceilf(depth * DepthMax));
}

inline float depth2Float(UINT32 depth) {
	return depth * (1.0f / DepthMax);
}
