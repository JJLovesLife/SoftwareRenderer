#pragma once

#include <DirectXMath.h>
#include <stdlib.h>

class MathHelper
{
public:
	static float RandF() { return (float)(rand()) / (float)RAND_MAX; }

	static float RandF(float a, float b) { return a + RandF() * (b - a); };

	static int Rand(int a, int b) { return a + rand() % (b - a); };

	template<typename T>
	static T Min(const T& a, const T& b) {
		return a < b ? a : b;
	}

	template<typename T>
	static T Max(const T& a, const T& b) {
		return a > b ? a : b;
	}

	template<typename T>
	static T Lerp(const T& a, const T& b, float s) {
		return a + s * (b - a);
	}

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high) {
		return x < low ? low : (x > high ? high : x);
	}

	static float AngleFromXY(float x, float y) {
		float theta = 0.0f;
		if (x >= 0.0f) {
			theta = atanf(y / x);
			if (theta < 0.0f)
				theta += DirectX::XM_2PI;
		}
		else
			theta = atanf(y / x) + DirectX::XM_PI;
		return theta;
	}

	static DirectX::XMVECTOR XM_CALLCONV SphericalToCartesian(float radius, float theta, float phi) {
		return DirectX::XMVectorSet(
			radius * sinf(phi) * cosf(theta),
			radius * cosf(phi),
			radius * sinf(phi) * sinf(theta),
			1.0f);
	}

	static DirectX::XMMATRIX XM_CALLCONV InverseTranspose(DirectX::FXMMATRIX M) {
		DirectX::XMMATRIX A = M;
		A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

		return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, A));
	}

	static constexpr const DirectX::XMFLOAT4X4 Identity4x4 = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f };

	static DirectX::XMVECTOR XM_CALLCONV RandUnitVec3() {
		const DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		while (true) {
			DirectX::XMVECTOR v = DirectX::XMVectorSet(
				MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);
			if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
				continue;
			return DirectX::XMVector3Normalize(v);
		}
	};
	
	static DirectX::XMVECTOR XM_CALLCONV RandHemisphereUnitVec3(DirectX::XMVECTOR n) {
		const DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		const DirectX::XMVECTOR Zero = DirectX::XMVectorZero();
		while (true) {
			DirectX::XMVECTOR v = DirectX::XMVectorSet(
				MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);
			if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
				continue;
			if (DirectX::XMVector3Less(DirectX::XMVector3Dot(n, v), Zero))
				continue;
			return DirectX::XMVector3Normalize(v);
		}
	};

	static const float constexpr Infinity = 3.4028234e+38F;
};