#pragma once

#include "SRDevice.h"

using namespace DirectX;

void myVS(const BYTE* vsInput, BYTE* vsOutput, const BYTE*const* constBuffer) {
	XMFLOAT4* posH = reinterpret_cast<XMFLOAT4*>(vsOutput);
	XMFLOAT4* color = reinterpret_cast<XMFLOAT4*>(vsOutput + sizeof(XMFLOAT4));

	const XMFLOAT3& posW = *reinterpret_cast<const XMFLOAT3*>(vsInput);
	const XMFLOAT4& colorI = *reinterpret_cast<const XMFLOAT4*>(vsInput + sizeof(XMFLOAT3));

	const XMFLOAT4X4& WVP = *reinterpret_cast<const XMFLOAT4X4*>(constBuffer[0]);

	XMStoreFloat4(posH, XMVector4Transform(XMVectorSet(posW.x, posW.y, posW.z, 1.0f), XMLoadFloat4x4(&WVP)));
	*color = colorI;
}

void myPS(BYTE* psInput, DirectX::XMFLOAT4* pixelColor, const BYTE*const* constBuffer) {
	XMFLOAT4 posH = *reinterpret_cast<XMFLOAT4*>(psInput);
	XMFLOAT4 color = *reinterpret_cast<XMFLOAT4*>(psInput + sizeof(XMFLOAT4));

	*pixelColor = color;
}

#ifdef TestQuadPS
void myQuadPS(BYTE* psInput[4], DirectX::XMFLOAT4 (*pixelColor)[4], const BYTE*const* constBuffer) {
	// just repeat the work 4 times to test rasterize works corret
	for (int i = 0; i < 4; i++) {
		XMFLOAT4 posH = *reinterpret_cast<XMFLOAT4*>(psInput[i]);
		XMFLOAT4 color = *reinterpret_cast<XMFLOAT4*>(psInput[i] + sizeof(XMFLOAT4));

		(*pixelColor)[i] = color;
	}
}
#endif