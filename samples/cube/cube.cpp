#include "SRDevice.h"
#include <DirectXColors.h>
#include <array>
#include "MathHelper.h"

//#define TestDepth
//#define TestQuadPS

#include "shader.h"

using namespace DirectX;

struct ObjectConstants {
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4;
};

class SRApp : public SRDevice
{
public:
	SRApp(HINSTANCE hInstance) : SRDevice(hInstance) {};
	~SRApp() {};

	virtual bool Initialize() override;

private:
	virtual void Update(const GameTimer& gt) override;
	virtual void DrawScene(const GameTimer& gt) override;
	virtual void OnResize();

	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	
	void OnKeyboardInput(const GameTimer& gt);

	void ResizeTarget(UINT width, UINT height);
	void SetDebugMode(bool newDebugMode, UINT newDebugLevel);

	SRResourceHandle mBackHandle = SRDevice::InvalidHandle;
	SRResourceHandle mDepthStencilHandle = SRDevice::InvalidHandle;

	SRResourceHandle mVertexBuffer = SRDevice::InvalidHandle;
	SRResourceHandle mIndexBuffer = SRDevice::InvalidHandle;
	SRResourceHandle mConstBuffer = SRDevice::InvalidHandle;

	UINT mTargetWidth = 0;
	UINT mTargetHeight = 0;

	bool mDebugMode = false;
	UINT mDebugLevel = 0;

	POINT mLastMousePos;

	float mTheta = 0.5f * XM_PI;
	float mPhi = XM_PIDIV2;
	float mRadius = 5.0f;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4;
	XMFLOAT4X4 mView = MathHelper::Identity4x4;
	XMFLOAT4X4 mProj = MathHelper::Identity4x4;

	ObjectConstants mObjectCB;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		SRApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

bool SRApp::Initialize() {
	if (!SRDevice::Initialize())
		return false;
	
#if defined(DEBUG) || defined(_DEBUG)
	SREnableDebugLayer();
#endif

	if (!SRAllocateResource(5))
		return false;

	mTargetWidth = GetClientWidth();
	mTargetHeight = GetClientHeight();

	SRResourceDescription desc;
	desc.DIMENSION = SRResourceDimensionTexture2D;
	desc.FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.WIDTH = mTargetWidth;
	desc.HEIGHT = mTargetHeight;
	desc.DEPTH = 1;
	if (!SRCreateResource(desc, &mBackHandle))
		return false;

	desc.FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;
	if (!SRCreateResource(desc, &mDepthStencilHandle))
		return false;


	struct Vertex {
		XMFLOAT3 Pos;
		XMFLOAT4 Color;
	};

	std::array<Vertex, 8> vertices = {
#ifdef TestDepth
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+0.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(+0.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Yellow) }),
#else
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
#endif
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint32_t, 36> indices = {
		// front face
		0, 1, 2,
		0, 2, 3,
		// back face
		4, 6, 5,
		4, 7, 6,
		// left face
		4, 5, 1,
		4, 1, 0,
		// right face
		3, 2, 6,
		3, 6, 7,
		// top face
		1, 5, 6,
		1, 6, 2,
		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	desc.DIMENSION = SRResourceDimensionBuffer;
	desc.FORMAT = DXGI_FORMAT_UNKNOWN;
	desc.WIDTH = UINT(sizeof(Vertex) * vertices.size());
	desc.HEIGHT = 1;
	if (!SRCreateResource(desc, &mVertexBuffer))
		return false;
	SRCopyToResource(mVertexBuffer, vertices.data(), UINT(sizeof(Vertex) * vertices.size()));
	SRIASetVertexBuffers(mVertexBuffer);

	desc.WIDTH = UINT(sizeof(Vertex) * indices.size());
	if (!SRCreateResource(desc, &mIndexBuffer))
		return false;
	SRCopyToResource(mIndexBuffer, indices.data(), UINT(sizeof(Vertex) * indices.size()));
	SRIASetIndexBuffers(mIndexBuffer);


	desc.WIDTH = UINT(sizeof(XMFLOAT4X4));
	if (!SRCreateResource(desc, &mConstBuffer))
		return false;
	SRIASetConstantBuffers(0, mConstBuffer);


	SRPipelineState mPSO;
	mPSO.VSInputByteStride = 7 * sizeof(float);
	mPSO.VSOutputByteCount = 8 * sizeof(float);
	mPSO.VS = &myVS;
	mPSO.PS = &myPS;
	mPSO.NumConstantBuffer = 1;
	mPSO.EnableZPrePass = true;
	mPSO.EnableQuadPixelShader = false;
#ifdef TestQuadPS
	mPSO.EnableQuadPixelShader = true;
	mPSO.QuadPS = &myQuadPS;
#endif
	SRSetPipelineState(mPSO);

	return true;
}

void SRApp::Update(const GameTimer& gt) {
	OnKeyboardInput(gt);

	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMMATRIX view = XMMatrixLookAtRH(
		XMVectorSet(x, y, z, 1.0f),
		XMVectorZero(),
		g_XMIdentityR1.v);

	XMStoreFloat4x4(&mView, view);

	XMMATRIX wordViewProj = XMLoadFloat4x4(&mWorld) * view * XMLoadFloat4x4(&mProj);

	XMStoreFloat4x4(&mObjectCB.WorldViewProj, wordViewProj);
	
	SRCopyToResource(mConstBuffer, &mObjectCB, sizeof(mObjectCB));
}

void SRApp::OnKeyboardInput(const GameTimer& gt) {
	bool newDebugMode;
	UINT newDebugLevel;
	if (GetDebugMode()) {
		newDebugMode = true;
		newDebugLevel = mDebugLevel;
		if (GetAsyncKeyState('1') & 0x8000)
			newDebugLevel = 1;
		else if (GetAsyncKeyState('2') & 0x8000)
			newDebugLevel = 2;
		else if (GetAsyncKeyState('3') & 0x8000)
			newDebugLevel = 3;
		else if (GetAsyncKeyState('4') & 0x8000)
			newDebugLevel = 4;
		else if (GetAsyncKeyState('5') & 0x8000)
			newDebugLevel = 5;
		else if (GetAsyncKeyState('6') & 0x8000)
			newDebugLevel = 6;
		else if (GetAsyncKeyState('7') & 0x8000)
			newDebugLevel = 7;
		else if (GetAsyncKeyState('8') & 0x8000)
			newDebugLevel = 8;
	}
	else {
		newDebugMode = false;
		newDebugLevel = 0;
	}

	SetDebugMode(newDebugMode, newDebugLevel);
}

void SRApp::SetDebugMode(bool newDebugMode, UINT newDebugLevel) {
	if (newDebugMode != mDebugMode || newDebugLevel != mDebugLevel) {
		mDebugMode = newDebugMode;
		mDebugLevel = mDebugMode ? (newDebugLevel > 0 ? newDebugLevel : 1) : 0;

		if (mDebugLevel > 1)
			ResizeTarget(GetClientWidth() / mDebugLevel, GetClientHeight() / mDebugLevel);
		else
			ResizeTarget(GetClientWidth(), GetClientHeight());

		SRSetMagMode(mDebugMode, mDebugLevel);
	}
}

void SRApp::DrawScene(const GameTimer& gt) {
	SRClearRenderTargetView(mBackHandle, Colors::Black);
	SRClearDepthStencilView(mDepthStencilHandle, SRClearFlagDepthStencil, 1.0, 0);
	SROMSetRenderTarget(mBackHandle, mDepthStencilHandle, true);
#ifdef TestDepth
	SRDrawInstanced(3 * 2, 1, 0, 0);
#else
	SRDrawIndexedInstanced(3 * 2 * 6, 1, 0, 0, 0);
#endif
}

void SRApp::OnResize() {
	SRDevice::OnResize();

	if (mBackHandle != SRDevice::InvalidHandle &&
		(mTargetWidth != GetClientWidth() ||
			mTargetHeight != GetClientHeight()))
	{
		if (mDebugLevel > 1)
			ResizeTarget(GetClientWidth() / mDebugLevel, GetClientHeight() / mDebugLevel);
		else
			ResizeTarget(GetClientWidth(), GetClientHeight());
	}

	XMStoreFloat4x4(&mProj, XMMatrixPerspectiveFovRH(0.25f * XM_PI, GetAspectRatio(), 1.0f, 1000.0f));
}

void SRApp::ResizeTarget(UINT width, UINT height) {
	if (width != mTargetWidth || height != mTargetHeight) {
		mTargetWidth = width;
		mTargetHeight = height;
		SRResourceDescription desc;
		desc.DIMENSION = SRResourceDimensionTexture2D;
		desc.FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.WIDTH = mTargetWidth;
		desc.HEIGHT = mTargetHeight;
		desc.DEPTH = 1;
		SRResizeResource(mBackHandle, desc);

		desc.FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;
		SRResizeResource(mDepthStencilHandle, desc);
	}
}

void SRApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos = { x, y };

	SetCapture(GetMainWnd());
}

void SRApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void SRApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if (btnState & MK_LBUTTON) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta -= dx;
		mPhi += dy;

		mPhi = MathHelper::Clamp(mPhi, 0.1f, XM_PI - 0.1f);
	}
	else if (btnState & MK_RBUTTON) {
		// Make each pixel correspond to 0.005 unit in scene.
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		mRadius += dx - dy;

		mRadius = MathHelper::Clamp(mRadius, 1.0f, 15.0f);
	}

	mLastMousePos = { x, y };
}
