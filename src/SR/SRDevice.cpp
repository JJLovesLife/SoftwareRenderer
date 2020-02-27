#include "SRDevice.h"
#include "SRUtils.h"

#pragma warning(disable : 4018)

bool SRDevice::SRAllocateResource(UINT number) {
	if (number > mResources.size()) {
		mResources.resize(number);
	}
	else if (number < mResources.size()) {
		// the last handle with ptr != nullptr, which means there is resource at this handle
		SRResourceHandle lastHandle = SRResourceHandle(mResources.size() - 1);
		while (lastHandle >= number) {
			if (mResources[lastHandle].ptr != nullptr) {
				SRError(L"Unable to shrink resources heap");
				return false;
			}
			lastHandle--;
		}
		
		// safe to shrink
		mResources.resize(number);
		if (mInternalAllocateHelperHandle >= number)
			mInternalAllocateHelperHandle = 0;
	}
	return true;
}

bool SRDevice::FillResouceAttribute(const SRResourceDescription desc, SRResource& resource) {
	switch (desc.DIMENSION)
	{
	case SRResourceDimensionBuffer:
	case SRResourceDimensionTexture1D:
		if (desc.HEIGHT != 1 || desc.DEPTH != 1) return false;
		resource.WIDTH = desc.WIDTH;
		resource.HEIGHT = 1;
		resource.DEPTH = 1;
		break;

	case SRResourceDimensionTexture2D:
		if (desc.DEPTH != 1) return false;
		resource.WIDTH = desc.WIDTH;
		resource.HEIGHT = desc.HEIGHT;
		resource.DEPTH = 1;
		break;

	case SRResourceDimensionTexture3D:
		resource.WIDTH = desc.WIDTH;
		resource.HEIGHT = desc.HEIGHT;
		resource.DEPTH = desc.DEPTH;
		break;

	case SRResourceDimensionTextureCube:
		if (desc.DEPTH != 6) return false;
		resource.WIDTH = desc.WIDTH;
		resource.HEIGHT = desc.HEIGHT;
		resource.DEPTH = 6;
		break;

	default:
		SRFatal(L"Unsupport dimension.");
		return false;
	}
	resource.DIMENSION = desc.DIMENSION;
	resource.FORMAT = desc.FORMAT;
	return true;
}

bool SRDevice::SRCreateResource(SRResourceDescription Desc, SRResourceHandle* pHandle) {
	size_t total = mResources.size();
	for (UINT i = 0; i < total; i++) {
		if (mResources[mInternalAllocateHelperHandle].ptr == nullptr) {
			auto& resource = mResources[mInternalAllocateHelperHandle];
			if (!FillResouceAttribute(Desc, resource)) {
				*pHandle = InvalidHandle;
				SRError(L"Incoorect description.");
				return false;
			}

			UINT size = resource.WIDTH  * resource.HEIGHT * resource.DEPTH * SizeOfFormat(resource.FORMAT);
			if (size == 0) {
				// if size is 0, then we return true but not create a resource.
				*pHandle = InvalidHandle;
				return true;
			}
			resource.ptr = (BYTE*)malloc(size);

			// point to the next handle inorder to speed up following create

			if (resource.ptr == nullptr) {
				*pHandle = InvalidHandle;
				SRError(L"Out of memory");
				return false;
			}
			else {
				*pHandle = mInternalAllocateHelperHandle;
				mInternalAllocateHelperHandle = (mInternalAllocateHelperHandle + 1) % total;
				return true;
			}
		}
		mInternalAllocateHelperHandle = (mInternalAllocateHelperHandle + 1) % total;
	}
	*pHandle = InvalidHandle;
	SRError(L"No available handle.");
	return false;
}

void SRDevice::SRCopyToResource(SRResourceHandle Handle, const void* pData, UINT len) {
	if (Handle >= mResources.size() || mResources[Handle].ptr == nullptr) {
		SRError(L"Invalid Resource");
		return;
	}
	auto& resources = mResources[Handle];
	if (len > resources.WIDTH * resources.HEIGHT * resources.DEPTH) {
		SRError(L"Too long, out of border.");
		return; 
	}
	memcpy(mResources[Handle].ptr, pData, len);
	return;
}

void SRDevice::SRReleaseResource(SRResourceHandle Handle) {
	if (Handle < mResources.size()) {
		SRResource& resource = mResources[Handle];
		free(resource.ptr);
		resource.ptr = nullptr;
	}
}

bool SRDevice::SRResizeResource(SRResourceHandle Handle, SRResourceDescription Desc) {
	if (Handle >= mResources.size()) {
		SRError(L"Handle out of range.");
		return false;
	}
	SRResource& resource = mResources[Handle];
	if (resource.ptr == nullptr) {
		SRError(L"Invalid resource handle.");
		return false;
	}
	if (resource.DIMENSION != Desc.DIMENSION ||
		resource.FORMAT != Desc.FORMAT)
	{
		SRError(L"Description doesn't correspond resource.");
		return false;
	}
	if (!FillResouceAttribute(Desc, resource)) {
		SRError(L"Incorrect Description.");
	}
	UINT size = resource.WIDTH  * resource.HEIGHT * resource.DEPTH * SizeOfFormat(resource.FORMAT);
	if (size == 0) {
		free(resource.ptr);
		resource.ptr = nullptr;
		return true;
	}
	BYTE* newPtr = (BYTE*)realloc(resource.ptr, size);
	if (newPtr == nullptr) {
		SRError(L"Resize failed.");
		return false;
	}
	else {
		resource.ptr = newPtr;
		return true;
	}
}

void SRDevice::SREnableDebugLayer() {
	mDebugLayer = true;
}

inline bool SRDevice::ValidRenderTarget(const SRResourceHandle handle) {
	return handle < mResources.size() && ValidRenderTarget(mResources[handle]);
}

inline bool SRDevice::ValidRenderTarget(const SRResource& renderTarget) {
	// only support r8g8b8a8 format.
	return renderTarget.ptr != nullptr &&
		renderTarget.DIMENSION == SRResourceDimensionTexture2D &&
		renderTarget.FORMAT == DXGI_FORMAT_R8G8B8A8_UNORM &&
		renderTarget.DEPTH == 1;
}

inline bool SRDevice::ValidDepthStencil(const SRResourceHandle handle) {
	return handle < mResources.size() && ValidDepthStencil(mResources[handle]);
}

inline bool SRDevice::ValidDepthStencil(const SRResource& depth) {
	// only support d24_unorm_s8_uint format.
	return depth.ptr != nullptr &&
		depth.DIMENSION == SRResourceDimensionTexture2D &&
		depth.FORMAT == DXGI_FORMAT_D24_UNORM_S8_UINT &&
		depth.DEPTH == 1;
}

inline bool SRDevice::ValidDepthStencil(const SRResource& depth, UINT width, UINT height) {
	// only support d24_unorm_s8_uint format.
	return depth.ptr != nullptr &&
		depth.DIMENSION == SRResourceDimensionTexture2D &&
		depth.FORMAT == DXGI_FORMAT_D24_UNORM_S8_UINT &&
		depth.WIDTH == width &&
		depth.HEIGHT == height &&
		depth.DEPTH == 1;
}

inline bool SRDevice::ValidDrawTarget(const SRResourceHandle target, const SRResourceHandle depth) {
	if (!ValidRenderTarget(target) ||
		!ValidDepthStencil(depth))
		return false;

	auto& targetResource = mResources[target];
	auto& depthResource = mResources[depth];
	if (depthResource.WIDTH != targetResource.WIDTH ||
		depthResource.HEIGHT != targetResource.HEIGHT)
		return false;

	return true;
}

void SRDevice::ResizeRenderTarget(const SRResource renderTarget) {
	assert(ValidRenderTarget(renderTarget));
	UINT width = renderTarget.WIDTH;
	UINT height = renderTarget.HEIGHT;
	
	if (width != mInternalRenderTargetWidth ||
		height != mInternalRenderTargetHeight)
	{
		mInternalRenderTargetWidth = width;
		mInternalRenderTargetHeight = height;

		FlushCommandQueue();

		if (width * height == 0) {
			for (UINT i = 0; i < mInternalSwapChainNum; i++) {
				mRenderTargetHelper[i].Reset();
			}
			free(mInternalHiZCache);
			return;
		}

		for (UINT i = 0; i < mInternalSwapChainNum; i++) {
			mRenderTargetHelper[i].Reset();
			TF(md3dDevice->CreateCommittedResource(
				mUMA ?
				&CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE, D3D12_MEMORY_POOL_L0) :
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
					width, height, 1, 1),
				mUMA ?
				D3D12_RESOURCE_STATE_GENERIC_READ :
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(mRenderTargetHelper[i].GetAddressOf())));

			if (!mUMA) {
				mRenderTargetUploader[i].Reset();
				const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRenderTargetHelper[i].Get(), 0, 1);
				TF(md3dDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(mRenderTargetUploader[i].GetAddressOf())));
			}
		}

		CreateMagDescriptor();

		// HiZ cache
		// I am not going to alloc UINT32 for every value instead of UINT24,
		// cause it increase reading time and more complex.
		if (mInternalHiZCache != nullptr) {
			mInternalHiZCache = (UINT32*)realloc(mInternalHiZCache, ((width + 7) / 8) * ((height + 7) / 8) * sizeof(UINT32) * 2);
		}
		else {
			mInternalHiZCache = (UINT32*)malloc(((width + 7) / 8) * ((height + 7) / 8) * sizeof(UINT32) * 2);
		}

		if (mInternalHiZCache == nullptr) {
			SRFatal(L"HiZ cache alloc error.");
			return;
		}
	}
}

void SRDevice::SRClearRenderTargetView(SRResourceHandle ResourceHandle, const float color[4]) {
	if (!ValidRenderTarget(ResourceHandle)) {
		SRError(L"Invalid render target hanlde");
		return;
	}

	SRResource& renderTarget = mResources[ResourceHandle];
	const UINT Step = renderTarget.HEIGHT * renderTarget.WIDTH / 8;
	const int Left = renderTarget.HEIGHT * renderTarget.WIDTH % 8;
	BYTE colors[] = {
		BYTE(clamp(color[0]) * 255),
		BYTE(clamp(color[1]) * 255),
		BYTE(clamp(color[2]) * 255),
		BYTE(clamp(color[3]) * 255) };

#pragma omp parallel for num_threads(8)
	for (int id = 0; id < 8; id++) {
		const UINT Count = id < Left ? Step + 1 : Step;
		BYTE* image = renderTarget.ptr + (id * Step + min(Left, id)) * 4;
		for (UINT i = 0; i < Count; i++) {
			image[0] = colors[0];
			image[1] = colors[1];
			image[2] = colors[2];
			image[3] = colors[3];
			image += 4;
		}
	}
}

void SRDevice::SRClearDepthStencilView(SRResourceHandle ResourceHandle,
	SRClearFlags flag, float depth, UINT8 stencil)
{
	if (!ValidDepthStencil(ResourceHandle)) {
		SRError(L"Invalid DepthStencil handle.");
		return;
	}

	UINT32 mask = 0;
	switch (flag) {
	case SRClearFlagDepth:
		mask = 0xffffff00;
		break;
	case SRClearFlagStencil:
		mask = 0xff;
		break;
	case SRClearFlagDepthStencil:
		mask = 0xffffffff;
		break;
	default:
		SRError(L"Unknwon Flag.");
		return;
	}
	UINT32 depth24 = float2Depth(clamp(depth));
	UINT32 data = ((depth24 << 8) + UINT32(stencil)) & mask;

	SRResource& depthStencil = mResources[ResourceHandle];
	const UINT Step = depthStencil.HEIGHT * depthStencil.WIDTH / 8;
	const UINT Left = depthStencil.HEIGHT * depthStencil.WIDTH % 8;

#pragma omp parallel for num_threads(8)
	for (int id = 0; id < 8; id++) {
		const UINT Count = id < Left ? Step + 1 : Step;
		UINT32* image = reinterpret_cast<UINT32*>(depthStencil.ptr) + id * Step + min(Left, id);
		for (UINT i = 0; i < Count; i++) {
			*image = *image & ~mask | data;
			image++;
		}
	}
}

void SRDevice::SRSetPipelineState(SRPipelineState PipelineState) {
	mPipelineState = PipelineState;
}

void SRDevice::SRIASetVertexBuffers(SRResourceHandle ResourceHandle) {
	if (ResourceHandle >= mResources.size()) {
		SRError(L"Invalid handle.");
		mVertexBufferHandle = InvalidHandle;
		return;
	}
	
	auto& resource = mResources[ResourceHandle];
	if (resource.ptr == nullptr ||
		resource.DIMENSION != SRResourceDimensionBuffer ||
		resource.HEIGHT != 1 ||
		resource.DEPTH != 1) {
		SRError(L"Invalid vertex buffer.");
		mVertexBufferHandle = InvalidHandle;
		return;
	}
	mVertexBufferHandle = ResourceHandle;
}

void SRDevice::SRIASetIndexBuffers(SRResourceHandle ResourceHandle) {
	if (ResourceHandle >= mResources.size()) {
		SRError(L"Invalid handle.");
		mIndexBufferHandle = InvalidHandle;
		return;
	}

	auto& resource = mResources[ResourceHandle];
	if (resource.ptr == nullptr ||
		resource.DIMENSION != SRResourceDimensionBuffer ||
		resource.HEIGHT != 1 ||
		resource.DEPTH != 1) {
		SRError(L"Invalid index buffer.");
		mIndexBufferHandle = InvalidHandle;
		return;
	}
	mIndexBufferHandle = ResourceHandle;
}

void SRDevice::SRIASetConstantBuffers(UINT Index, SRResourceHandle ResourceHandle) {
	if (Index >= 8) {
		SRError(L"Only support 8 constant buffers.");
		return;
	}
	if (ResourceHandle >= mResources.size() ||
		mResources[ResourceHandle].ptr == nullptr)
	{
		SRError(L"Invalid handle.");
		return;
	}
	mConstantsBufferHandle[Index] = ResourceHandle;
}

void SRDevice::SRIASetPrimitiveTopology(SRPrimitiveTopology Primitive) {
	if (SRPrimitiveCount >= SRPrimitiveCount) {
		SRError(L"Unknown Primitive.");
		return;
	}
	mPrimitive = Primitive;
}

void SRDevice::SROMSetRenderTarget(SRResourceHandle TargetHandle, SRResourceHandle DepthHandle, bool IsAllDepthInitToOne) {
	if (DepthHandle == InvalidHandle ||
		DepthHandle >= mResources.size() ||
		!ValidRenderTarget(TargetHandle))
	{
		mRenderTargetHandle = InvalidHandle;
		mDepthStencilHandle = InvalidHandle;
		SRError(L"Invalid render target handle or depth handle");
	}
	else {
		if (ValidDrawTarget(TargetHandle, DepthHandle)) {
			mRenderTargetHandle = TargetHandle;
			mDepthStencilHandle = DepthHandle;
			ResizeRenderTarget(mResources[mRenderTargetHandle]);
			InitHiZCache(IsAllDepthInitToOne);
		}
		else {
			mRenderTargetHandle = InvalidHandle;
			mDepthStencilHandle = InvalidHandle;
			SRError(L"The depth buffer size does not correspond to target buffer.");
		}
	}
}

void SRDevice::InitHiZCache(bool isAllDepthInitToOne) {
	auto& depthStencil = mResources[mDepthStencilHandle];
	const UINT HiZWidth = (depthStencil.WIDTH + 7) / 8;
	const UINT HiZHeight = (depthStencil.HEIGHT + 7) / 8;
	const UINT StepHiZ = HiZWidth * HiZHeight / 8;
	const UINT LeftHiZ = HiZWidth * HiZHeight % 8;

	if (isAllDepthInitToOne) {
		const UINT32 depth24 = DepthMax;
#pragma omp parallel for num_threads(8)
		for (int id = 0; id < 8; id++) {
			const UINT Count = id < LeftHiZ ? StepHiZ + 1 : StepHiZ;
			UINT32* image = mInternalHiZCache + (id * StepHiZ + min(LeftHiZ, id)) * 2;
			for (UINT i = 0; i < Count; i++) {
				image[0] = depth24;	// min
				image[1] = depth24;	// max
				image += 2;
			}
		}
	}
	else {
#pragma omp parallel for num_threads(8)
		for (int id = 0; id < 8; id++) {
			const UINT Count = id < LeftHiZ ? StepHiZ + 1 : StepHiZ;
			const UINT Base = (id * StepHiZ + min(LeftHiZ, id));
			UINT32* image = mInternalHiZCache + Base * 2;
			for (UINT i = 0; i < Count; i++) {
				int tileX = int((Base + i) % HiZWidth) * 8;
				int tileY = int((Base + i) / HiZWidth) * 8;

				UINT32 minDepth = DepthMax;
				UINT32 maxDepth = 0;
				
				// stupid implementation since I will not really run this subprogram in my project
				for (int i = 0; i < 8; i++) {
					for (int j = 0; j < 8; j++) {
						if ((tileX + i) >= depthStencil.WIDTH ||
							(tileY + j) >= depthStencil.HEIGHT)
							continue;

						UINT32 pixelDepth = *(reinterpret_cast<UINT32*>(depthStencil.ptr) +
							(tileY + j) * depthStencil.WIDTH + (tileX + i)) >> 8;

						if (pixelDepth > maxDepth) {
							maxDepth = pixelDepth;
						}
						if (pixelDepth < minDepth) {
							minDepth = pixelDepth;
						}
					}
				}

				image[0] = minDepth;	// min
				image[1] = maxDepth;	// max
				image += 2;
			}
		}
	}
}

bool SRDevice::Initialize() {
	mMainWndCaption = L"SR";
	if (!D3DApp::Initialize())
		return false;

	for (int i = 0; i < 8; i++) {
		mConstantsBufferHandle[i] = InvalidHandle;
	}

	for (int i = 0; i < mInternalSwapChainNum; i++) {
		mInternalFences[i] = 0;
		TF(md3dDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mInternalCmdListAllocs[i].GetAddressOf())));
	}

	InitializeMagPresent();

	//mUMA = !mUMA; //uncomment this line to test the other case
	return true;
}

void SRDevice::Draw(const GameTimer& gt) {
	DrawScene(gt);


	mInternalSwapChainIndex = (mInternalSwapChainIndex + 1) % mInternalSwapChainNum;
	UINT64 prevFence= mInternalFences[mInternalSwapChainIndex];
	if (mFence->GetCompletedValue() < prevFence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		TF(mFence->SetEventOnCompletion(prevFence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}


	auto& cmdListAlloc = mInternalCmdListAllocs[mInternalSwapChainIndex];
	auto& helperResource = mRenderTargetHelper[mInternalSwapChainIndex];
	auto& uploaderResource = mRenderTargetUploader[mInternalSwapChainIndex];

	TF(cmdListAlloc->Reset());
	if (ValidRenderTarget(mRenderTargetHandle)) {
		SRResource& renderTarget = mResources[mRenderTargetHandle];
		UINT rowPitch = sizeof(renderTarget.FORMAT) * renderTarget.WIDTH;
		UINT depthPitch = rowPitch * renderTarget.HEIGHT;

		mCommandList->Reset(cmdListAlloc.Get(), nullptr);

		if (mUMA) {
			helperResource->WriteToSubresource(0, nullptr, renderTarget.ptr, rowPitch, depthPitch);
		}
		else {
			D3D12_SUBRESOURCE_DATA data;
			data.pData = renderTarget.ptr;
			data.RowPitch = rowPitch;
			data.SlicePitch = depthPitch;
			UpdateSubresources(mCommandList.Get(),
				helperResource.Get(), uploaderResource.Get(), 0, 0, 1, &data);
		}

		if (!mMagPresent) {
			DirectPresent(helperResource.Get());
		}
		else {
			MagnificationPresent(helperResource.Get());
		}

		TF(mCommandList->Close());

		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		TF(mSwapChain->Present(0, GetAllowTearing() ? DXGI_PRESENT_ALLOW_TEARING : 0));
		mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

		mInternalFences[mInternalSwapChainIndex] = ++mCurrentFence;
		mCommandQueue->Signal(mFence.Get(), mCurrentFence);
	}
}

void SRDevice::DirectPresent(ID3D12Resource* presentResource) {
	if (!mUMA) {
		mCommandList->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				presentResource, D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_COPY_SOURCE));
	}

	D3D12_TEXTURE_COPY_LOCATION src, dest;
	src.pResource = presentResource;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	dest.pResource = CurrentBackBuffer();
	dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dest.SubresourceIndex = 0;
	D3D12_BOX box = { 0, 0, 0,
		min(UINT(mClientWidth), UINT(mInternalRenderTargetWidth)),
		min(UINT(mClientHeight),UINT(mInternalRenderTargetHeight)), 1 };
	mCommandList->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

	if (mUMA) {
		mCommandList->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT));
	}
	else {
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				presentResource, D3D12_RESOURCE_STATE_COPY_SOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT)
		};
		mCommandList->ResourceBarrier(2, barriers);
	}
}

SRDevice::~SRDevice() {
	for (auto& resource : mResources) {
		free(resource.ptr);
	}
	free(mInternalHiZCache);

	// flush at here, otherwise render target helper will be release before all command execute.
	FlushCommandQueue();
}
