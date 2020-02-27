#include "SRDevice.h"
#include "d3dUtil.h"

using namespace Microsoft::WRL;

void SRDevice::SRSetMagMode(bool EnableMag, UINT MagLevel) {
	if (!EnableMag || MagLevel == 0) {
		mMagPresent = false;
		mMagLevel = 0;
	}
	else {
		mMagPresent = true;
		mMagLevel = MagLevel;
	}
}

void SRDevice::InitializeMagPresent() {
	// descriptor heaps
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = mInternalSwapChainNum;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	
	TF(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mMagHeap)));

	// root signature
	CD3DX12_DESCRIPTOR_RANGE table;
	table.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].InitAsConstants(4, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &table);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
	}
	TF(hr);

	TF(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mMagRootSignature)));

	// shaders
	mMagTriVS = CompileShader(L"src\\SR\\Magnification\\Magnification.hlsl", nullptr, "TextureVS", "vs_5_1");
	mMagTriPS = CompileShader(L"src\\SR\\Magnification\\Magnification.hlsl", nullptr, "TexturePS", "ps_5_1");
	mMagLineVS = CompileShader(L"src\\SR\\Magnification\\Magnification.hlsl", nullptr, "LineVS", "vs_5_1");
	mMagLinePS = CompileShader(L"src\\SR\\Magnification\\Magnification.hlsl", nullptr, "LinePS", "ps_5_1");
	
	// PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC triPsoDesc = {};
	triPsoDesc.InputLayout = { nullptr, 0 };
	triPsoDesc.pRootSignature = mMagRootSignature.Get();
	triPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mMagTriVS->GetBufferPointer()),
		mMagTriVS->GetBufferSize()
	};
	triPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mMagTriPS->GetBufferPointer()),
		mMagTriPS->GetBufferSize()
	};
	triPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	triPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	triPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	triPsoDesc.DepthStencilState.DepthEnable = false;
	triPsoDesc.SampleMask = UINT_MAX;
	triPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	triPsoDesc.NumRenderTargets = 1;
	triPsoDesc.RTVFormats[0] = mBackBufferFormat;
	triPsoDesc.SampleDesc.Count = 1;
	triPsoDesc.SampleDesc.Quality = 0;
	triPsoDesc.DSVFormat = mDepthStencilFormat;

	TF(md3dDevice->CreateGraphicsPipelineState(&triPsoDesc, IID_PPV_ARGS(&mMagTriPSO)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC linePsoDesc = triPsoDesc;
	linePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mMagLineVS->GetBufferPointer()),
		mMagLineVS->GetBufferSize()
	};
	linePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mMagLinePS->GetBufferPointer()),
		mMagLinePS->GetBufferSize()
	};
	linePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	TF(md3dDevice->CreateGraphicsPipelineState(&linePsoDesc, IID_PPV_ARGS(&mMagLinePSO)));
}

void SRDevice::CreateMagDescriptor() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mMagHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < mInternalSwapChainNum; i++) {
		md3dDevice->CreateShaderResourceView(mRenderTargetHelper[i].Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}
}


void SRDevice::MagnificationPresent(ID3D12Resource* presentResource) {
	if (mUMA) {
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
	}
	else {
		CD3DX12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(
				presentResource, D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		};
		mCommandList->ResourceBarrier(2, barriers);
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());


	ID3D12DescriptorHeap* descriptorHeaps[] = { mMagHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// magnification the texture
	mCommandList->SetPipelineState(mMagTriPSO.Get());
	mCommandList->SetGraphicsRootSignature(mMagRootSignature.Get());
	mCommandList->SetGraphicsRoot32BitConstant(0, mMagLevel, 0);
	CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(mMagHeap->GetGPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(mInternalSwapChainIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, hDescriptor);

	mCommandList->IASetVertexBuffers(0, 0, nullptr);
	mCommandList->IASetIndexBuffer(nullptr);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawInstanced(6, 1, 0, 0);

	// draw the tile outline
	mCommandList->SetPipelineState(mMagLinePSO.Get());
	UINT32 screenSize[2] = { UINT32(mClientWidth), UINT32(mClientHeight) };
	mCommandList->SetGraphicsRoot32BitConstants(0, 2, screenSize, 1);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// horizontal
	mCommandList->SetGraphicsRoot32BitConstant(0, UINT(true), 3);
	mCommandList->DrawInstanced(2, (mClientHeight - 1) / (8 * mMagLevel) + 1, 0, 0);

	// vertical
	mCommandList->SetGraphicsRoot32BitConstant(0, UINT(false), 3);
	mCommandList->DrawInstanced(2, (mClientWidth - 1) / (8 * mMagLevel) + 1, 0, 0);

	if (mUMA) {
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));
	}
	else {
		CD3DX12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(
				CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT),
			CD3DX12_RESOURCE_BARRIER::Transition(
				presentResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COPY_DEST)
		};
		mCommandList->ResourceBarrier(2, barriers);
	}
}
