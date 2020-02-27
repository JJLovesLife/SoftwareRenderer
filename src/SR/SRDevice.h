#pragma once

#include "d3dApp.h"
#include "SRenum.h"

typedef struct SRResource{
	BYTE* ptr;
	UINT WIDTH;
	UINT HEIGHT;
	UINT DEPTH;
	DXGI_FORMAT FORMAT;
	SRResourceDimension DIMENSION;
} SRResource;

typedef UINT SRResourceHandle;

typedef struct SRResourceDescription {
	UINT WIDTH;
	UINT HEIGHT;
	UINT DEPTH;
	DXGI_FORMAT FORMAT;
	SRResourceDimension DIMENSION;
} SRResourceDescription;

typedef struct SRBlendDesc {
	bool BlendEnable;
	SRBlend SrcBlend;
	SRBlend DestBlend;
	SRBlendOp BlendOp;
	SRBlend SrcBlendAlpha;
	SRBlend DestBlendAlpha;
	SRBlendOp BlendOpAlpha;
	UINT8 RenderTargetWriteMask;
} SRBlendDesc;

/*
 * only support float format in intermedia data.
 * the first 16 bytes of vsOutput will be interpreted as SV_POSITION.
 */
typedef struct SRPipelineState {
	void (*VS)(const BYTE* vsInput, BYTE* vsOutput, const BYTE*const* constBuffer) = nullptr;
	void (*PS)(BYTE* psInput, DirectX::XMFLOAT4* pixelColor, const BYTE*const* constBuffer) = nullptr;
	UINT VSInputByteStride = 0;
	UINT VSOutputByteCount = 0;
	UINT NumConstantBuffer = 0;
	bool EnableZPrePass = false;
	bool EnableQuadPixelShader = false;
	void(*QuadPS)(BYTE* psInput[4], DirectX::XMFLOAT4 (*pixelColor)[4], const BYTE*const* constBuffer) = nullptr;
} SRPipelineState;

class SRDevice : D3DApp 
{
public:
	SRDevice() = delete;
	SRDevice(const SRDevice& rhs) = delete;
	SRDevice& operator=(const SRDevice& rhs) = delete;
	SRDevice(HINSTANCE hInstance) : D3DApp(hInstance) {};
	~SRDevice();
	virtual bool Initialize();

public:
	/*
	 * Software Renderer API
	 */
	// Resource API
	bool SRAllocateResource(UINT number);
	bool SRCreateResource(SRResourceDescription Desc, SRResourceHandle* pHandle);
	void SRCopyToResource(SRResourceHandle Handle, const void * pData, UINT len);
	void SRReleaseResource(SRResourceHandle Handle);
	bool SRResizeResource(SRResourceHandle Handle, SRResourceDescription Desc);

	// Debug API
	void SREnableDebugLayer();
	void SRSetMagMode(bool EnableMag, UINT MagLevel);

	// Render API
	void SRClearRenderTargetView(SRResourceHandle ResourceHandle, const float color[4]);
	void SRClearDepthStencilView(SRResourceHandle ResourceHandle,
		SRClearFlags flag, float depth, UINT8 stencil);

	void SRSetPipelineState(SRPipelineState PipelineState);

	void SRIASetVertexBuffers(SRResourceHandle ResourceHandle);
	void SRIASetIndexBuffers(SRResourceHandle ResourceHandle);
	void SRIASetConstantBuffers(UINT Index, SRResourceHandle ResourceHandle);
	void SRIASetPrimitiveTopology(SRPrimitiveTopology Primitive);

	void SROMSetRenderTarget(SRResourceHandle TargetHandle, SRResourceHandle DepthHandle, bool IsAllDepthInitToOne = false);

	void SRDrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void SRDrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation);

	/*
	 * Constants
	 */
	static const SRResourceHandle InvalidHandle = SRResourceHandle(-1);

private:
	/*
	 * Underlying Data
	 */
	std::vector<SRResource>  mResources;
	SRResourceHandle mRenderTargetHandle = InvalidHandle;
	SRResourceHandle mDepthStencilHandle = InvalidHandle;
	SRResourceHandle mVertexBufferHandle = InvalidHandle;
	SRResourceHandle mIndexBufferHandle = InvalidHandle;
	SRResourceHandle mConstantsBufferHandle[8];
	SRPipelineState mPipelineState;
	SRPrimitiveTopology mPrimitive = SRPrimitiveTopologyTriangleList;
	bool mDebugLayer = false;
	bool mMagPresent = false;
	UINT mMagLevel = 0;


	/*
	 * Internal variable
	 */
	SRResourceHandle mInternalAllocateHelperHandle = 0;
	UINT mInternalRenderTargetWidth = 0;
	UINT mInternalRenderTargetHeight = 0;
	static constexpr int mInternalSwapChainNum = 3;
	int mInternalSwapChainIndex = 0;
	UINT64 mInternalFences[mInternalSwapChainNum];
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mInternalCmdListAllocs[mInternalSwapChainNum];
	UINT32* mInternalHiZCache = nullptr;

private:
	/*
	 * helper function
	 */
	inline bool ValidRenderTarget(const SRResourceHandle handle);
	inline bool ValidRenderTarget(const SRResource& renderTarget);
	inline bool ValidDepthStencil(const SRResourceHandle handle);
	inline bool ValidDepthStencil(const SRResource& depth);
	inline bool ValidDepthStencil(const SRResource& depth, UINT width, UINT height);
	inline bool ValidDrawTarget(const SRResourceHandle target, const SRResourceHandle depth);
	void ResizeRenderTarget(const SRResource renderTarget);
	bool FillResouceAttribute(const SRResourceDescription desc, SRResource& resource);
	void InitHiZCache(bool isAllDepthInitToOne);

	void DirectPresent(ID3D12Resource* presentResource);
	void MagnificationPresent(ID3D12Resource* presentResource);
	void InitializeMagPresent();
	void CreateMagDescriptor();

	// rasterize helper function
	void DrawTriangle(const BYTE* vsInputs[3]);
	void RasterizeTriangle(const BYTE* vsOutput1, const BYTE* vsOutput2, const BYTE* vsOutput3, const BYTE*const* constBuffers);
	const BYTE*const* AssempleConstantBuffers();

private:
	/*
	 * D3D12 Interaction Layer
	 */
	virtual void Draw(const GameTimer& gt) override;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetUploader[mInternalSwapChainNum];
	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargetHelper[mInternalSwapChainNum];

	// Mag
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mMagHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mMagRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mMagTriVS = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mMagTriPS = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mMagLineVS = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mMagLinePS = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mMagTriPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mMagLinePSO;

	/*
	 * forward functions
	 */
public:
	int Run() { return D3DApp::Run(); };

protected:
	virtual void Update(const GameTimer& gt) = 0;
	virtual void DrawScene(const GameTimer& gt) = 0;
	virtual void OnResize() { D3DApp::OnResize(); };

	virtual void OnMouseDown(WPARAM btnState, int x, int y) {};
	virtual void OnMouseUp(WPARAM btnState, int x, int y) {};
	virtual void OnMouseMove(WPARAM btnState, int x, int y) {};

	int GetClientWidth() { return mClientWidth; };
	int GetClientHeight() { return mClientHeight; };
	float GetAspectRatio() const { return AspectRatio(); };
	HWND GetMainWnd() { return mhMainWnd; };
	bool GetDebugMode() { return mDebugMode; };
};