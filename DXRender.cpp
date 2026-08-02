#include "DXRender.hpp"

#include <DirectXTex.h>

#include <stdio.h>
#include <string.h>
#include <algorithm>

using namespace DirectX;

static void SetDebugName(ID3D12Object* obj, const char* name)
{
#if defined(_DEBUG)
	if (obj && name)
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#else
	(void)obj;
	(void)name;
#endif
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetBackBufferRtv() const
{
	return GetRtvCpu(m_backBufferRtvIndices[m_frameIndex]);
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetSceneRtv() const
{
	if (m_ownsSceneColor && m_sceneRtvIndex != UINT_MAX)
		return GetRtvCpu(m_sceneRtvIndex);
	return GetBackBufferRtv();
}

ID3D12Resource* DXRender::GetSceneColor() const
{
	if (m_ownsSceneColor && m_sceneColor)
		return m_sceneColor.Get();
	return GetBackBuffer();
}

UINT DXRender::GetDepthSrvIndex() const
{
	if (m_msaaCount > 1 && m_resolvedDepthSrvIndex != UINT_MAX)
		return m_resolvedDepthSrvIndex;
	return m_depthSrvIndex;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetDsv() const
{
	return GetDsvCpu(m_depthDsvIndex);
}

D3D12_VIEWPORT DXRender::MakeViewport() const
{
	D3D12_VIEWPORT vp = {};
	vp.Width = (float)m_width;
	vp.Height = (float)m_height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	return vp;
}

D3D12_RECT DXRender::MakeScissor() const
{
	D3D12_RECT r = { 0, 0, (LONG)m_width, (LONG)m_height };
	return r;
}

void DXRender::SetOpaqueState() { m_blendMode = BlendPassMode::Opaque; }
void DXRender::SetCutoutAlphaState() { m_blendMode = BlendPassMode::Cutout; }
void DXRender::SetSoftAlphaState() { m_blendMode = BlendPassMode::SoftAlpha; }

HRESULT DXRender::ChangeRasterizerStateToWireframe()
{
	m_rasterMode = RasterCullMode::Wireframe;
	return S_OK;
}

HRESULT DXRender::ChangeRasterizerStateToSolid()
{
	m_rasterMode = m_solidRasterMode;
	return S_OK;
}

void DXRender::ApplyRasterizerState() {}

void DXRender::SetCullNone()
{
	if (m_rasterMode != RasterCullMode::Wireframe)
		m_rasterMode = RasterCullMode::CullNone;
	m_solidRasterMode = RasterCullMode::CullNone;
}

void DXRender::SetCullFront()
{
	if (m_rasterMode != RasterCullMode::Wireframe)
		m_rasterMode = RasterCullMode::CullFront;
	m_solidRasterMode = RasterCullMode::CullFront;
}

void DXRender::SetVehicleRasterizer(bool wireframe)
{
	m_rasterMode = wireframe ? RasterCullMode::Wireframe : RasterCullMode::CullNone;
	if (!wireframe)
		m_solidRasterMode = RasterCullMode::CullNone;
}

UINT DXRender::AllocSrvIndex()
{
	if (m_srvCount >= kSrvHeapSize) {
		printf("Error: SRV heap exhausted (%u / %u)\n", m_srvCount, kSrvHeapSize);
		return UINT_MAX;
	}
	return m_srvCount++;
}

UINT DXRender::AllocRtvIndex()
{
	if (m_rtvCount >= kRtvHeapSize) {
		printf("Error: RTV heap exhausted\n");
		return UINT_MAX;
	}
	return m_rtvCount++;
}

UINT DXRender::AllocDsvIndex()
{
	if (m_dsvCount >= kDsvHeapSize) {
		printf("Error: DSV heap exhausted\n");
		return UINT_MAX;
	}
	return m_dsvCount++;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetSrvCpu(UINT index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)index * m_srvDescriptorSize;
	return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXRender::GetSrvGpu(UINT index) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE h = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)index * m_srvDescriptorSize;
	return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetRtvCpu(UINT index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)index * m_rtvDescriptorSize;
	return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetDsvCpu(UINT index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)index * m_dsvDescriptorSize;
	return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::GetSamplerCpu(UINT index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)index * m_samplerDescriptorSize;
	return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXRender::GetSamplerGpu(UINT index) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE h = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)index * m_samplerDescriptorSize;
	return h;
}

UINT DXRender::CreateTextureSrv(ID3D12Resource* resource, DXGI_FORMAT format)
{
	UINT index = AllocSrvIndex();
	if (index == UINT_MAX || !resource)
		return UINT_MAX;

	D3D12_RESOURCE_DESC rd = resource->GetDesc();
	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Format = (format != DXGI_FORMAT_UNKNOWN) ? format : rd.Format;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Texture2D.MipLevels = rd.MipLevels ? rd.MipLevels : 1;
	m_device->CreateShaderResourceView(resource, &srv, GetSrvCpu(index));
	return index;
}

UINT DXRender::CreateTexture2DArraySrv(ID3D12Resource* resource, DXGI_FORMAT format, UINT arraySize)
{
	UINT index = AllocSrvIndex();
	if (index == UINT_MAX || !resource)
		return UINT_MAX;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Format = format;
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srv.Texture2DArray.MipLevels = 1;
	srv.Texture2DArray.ArraySize = arraySize;
	m_device->CreateShaderResourceView(resource, &srv, GetSrvCpu(index));
	return index;
}

UINT DXRender::CreateTypedBufferSrv(ID3D12Resource* resource, UINT elementCount, UINT stride)
{
	UINT index = AllocSrvIndex();
	if (index == UINT_MAX || !resource || elementCount == 0 || stride == 0)
		return UINT_MAX;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Format = DXGI_FORMAT_UNKNOWN;
	srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv.Buffer.FirstElement = 0;
	srv.Buffer.NumElements = elementCount;
	srv.Buffer.StructureByteStride = stride;
	srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	m_device->CreateShaderResourceView(resource, &srv, GetSrvCpu(index));
	return index;
}

UINT DXRender::CreateAccelerationStructureSrv(ID3D12Resource* accelerationStructure)
{
	UINT index = AllocSrvIndex();
	if (index == UINT_MAX || !accelerationStructure)
		return UINT_MAX;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
	srv.Format = DXGI_FORMAT_UNKNOWN;
	srv.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.RaytracingAccelerationStructure.Location = accelerationStructure->GetGPUVirtualAddress();
	m_device->CreateShaderResourceView(nullptr, &srv, GetSrvCpu(index));
	return index;
}

UINT DXRender::CreateSampler(const D3D12_SAMPLER_DESC& desc)
{
	if (m_samplerCount >= kSamplerHeapSize) {
		printf("Error: sampler heap exhausted\n");
		return UINT_MAX;
	}
	UINT index = m_samplerCount++;
	m_device->CreateSampler(&desc, GetSamplerCpu(index));
	return index;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::CreateRtv(ID3D12Resource* resource, DXGI_FORMAT format)
{
	UINT index = AllocRtvIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = GetRtvCpu(index);
	if (format == DXGI_FORMAT_UNKNOWN) {
		m_device->CreateRenderTargetView(resource, nullptr, cpu);
	} else {
		D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
		rtv.Format = format;
		rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		m_device->CreateRenderTargetView(resource, &rtv, cpu);
	}
	return cpu;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRender::CreateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
{
	UINT index = AllocDsvIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = GetDsvCpu(index);
	m_device->CreateDepthStencilView(resource, desc, cpu);
	return cpu;
}

void DXRender::BindDescriptorHeaps()
{
	ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get(), m_samplerHeap.Get() };
	m_commandList->SetDescriptorHeaps(2, heaps);
}

void DXRender::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	if (!resource || before == after)
		return;
	D3D12_RESOURCE_BARRIER b = {};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = resource;
	b.Transition.StateBefore = before;
	b.Transition.StateAfter = after;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_commandList->ResourceBarrier(1, &b);
}

void DXRender::DeferRelease(IUnknown* obj)
{
	if (!obj)
		return;
	/* Release only after the fence signaled at the end of this frame
	 * (m_fenceValue + 1). Using the last signaled value frees one frame early. */
	DeferredRelease d;
	d.fenceValue = m_fenceValue + 1;
	d.obj = obj;
	m_deferred.push_back(d);
}

void DXRender::ProcessDeferredReleases(UINT64 completedFence)
{
	size_t write = 0;
	for (size_t i = 0; i < m_deferred.size(); i++) {
		if (m_deferred[i].fenceValue <= completedFence) {
			m_deferred[i].obj->Release();
		} else {
			m_deferred[write++] = m_deferred[i];
		}
	}
	m_deferred.resize(write);
}

void DXRender::WaitForGpu()
{
	const UINT64 fenceToWait = ++m_fenceValue;
	m_queue->Signal(m_fence.Get(), fenceToWait);
	if (m_fence->GetCompletedValue() < fenceToWait) {
		m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
	ProcessDeferredReleases(m_fence->GetCompletedValue());
}

HRESULT DXRender::CreateDevice()
{
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
			debug->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		return hr;

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
			break;
		adapter.Reset();
	}

	if (!adapter) {
		printf("Error: no D3D12 adapter\n");
		return E_FAIL;
	}

	hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr)) {
		printf("Error: D3D12CreateDevice failed\n");
		return hr;
	}
	SetDebugName(m_device.Get(), "DXRenderDevice");

	m_device.As(&m_device5);
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5 = {};
		if (SUCCEEDED(m_device->CheckFeatureSupport(
			D3D12_FEATURE_D3D12_OPTIONS5, &opts5, sizeof(opts5)))) {
			m_raytracingTier = opts5.RaytracingTier;
		}
		if (m_raytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
			printf("[Warn] DXR not supported on this GPU — RT shadows unavailable\n");
			m_device5.Reset();
		} else {
			printf("[Info] DXR tier %u available\n", (unsigned)m_raytracingTier);
		}
	}

	D3D12_COMMAND_QUEUE_DESC qd = {};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_queue));
	if (FAILED(hr))
		return hr;

	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(hr))
		return hr;
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_fenceEvent)
		return HRESULT_FROM_WIN32(GetLastError());

	return S_OK;
}

HRESULT DXRender::CreateHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
	rtvDesc.NumDescriptors = kRtvHeapSize;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	HRESULT hr = m_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap));
	if (FAILED(hr))
		return hr;

	D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
	dsvDesc.NumDescriptors = kDsvHeapSize;
	dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hr = m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap));
	if (FAILED(hr))
		return hr;

	D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
	srvDesc.NumDescriptors = kSrvHeapSize;
	srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = m_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_srvHeap));
	if (FAILED(hr))
		return hr;

	D3D12_DESCRIPTOR_HEAP_DESC sampDesc = {};
	sampDesc.NumDescriptors = kSamplerHeapSize;
	sampDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	sampDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = m_device->CreateDescriptorHeap(&sampDesc, IID_PPV_ARGS(&m_samplerHeap));
	if (FAILED(hr))
		return hr;

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	return S_OK;
}

HRESULT DXRender::CreateSwapChain(HWND hWnd)
{
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ComPtr<IDXGIFactory4> factory;
	HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		return hr;

	DXGI_SWAP_CHAIN_DESC1 sd = {};
	sd.BufferCount = kFrameCount;
	sd.Width = m_width;
	sd.Height = m_height;
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swap1;
	hr = factory->CreateSwapChainForHwnd(m_queue.Get(), hWnd, &sd, nullptr, nullptr, &swap1);
	if (FAILED(hr)) {
		printf("Error: CreateSwapChainForHwnd failed (0x%08X)\n", (unsigned)hr);
		return hr;
	}
	factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
	hr = swap1.As(&m_swapChain);
	if (FAILED(hr))
		return hr;

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	return S_OK;
}

HRESULT DXRender::CreateFrameResources()
{
	for (UINT i = 0; i < kFrameCount; i++) {
		HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
		if (FAILED(hr))
			return hr;
		m_backBufferRtvIndices[i] = AllocRtvIndex();
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, GetRtvCpu(m_backBufferRtvIndices[i]));
		m_backBufferState[i] = D3D12_RESOURCE_STATE_PRESENT;

		hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frames[i].allocator));
		if (FAILED(hr))
			return hr;
	}

	HRESULT hr = m_device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_frames[m_frameIndex].allocator.Get(),
		nullptr, IID_PPV_ARGS(&m_commandList));
	if (FAILED(hr))
		return hr;
	m_commandList->Close();

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_uploadAllocator));
	if (FAILED(hr))
		return hr;
	hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_uploadAllocator.Get(),
		nullptr, IID_PPV_ARGS(&m_uploadList));
	if (FAILED(hr))
		return hr;
	m_uploadList->Close();
	m_uploadOpen = false;
	return S_OK;
}

UINT DXRender::PickMSAACount(DXGI_FORMAT format)
{
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS ms = {};
	ms.Format = format;
	ms.SampleCount = kDesiredMSAA;
	ms.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	if (SUCCEEDED(m_device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms, sizeof(ms))) &&
		ms.NumQualityLevels > 0)
		return kDesiredMSAA;

	ms.SampleCount = 2;
	if (SUCCEEDED(m_device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &ms, sizeof(ms))) &&
		ms.NumQualityLevels > 0)
		return 2;

	return 1;
}

HRESULT DXRender::CreateMSAAColor()
{
	m_ownsSceneColor = false;
	m_sceneColor.Reset();
	m_sceneRtvIndex = UINT_MAX;
	m_sceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	if (m_msaaCount <= 1)
		return S_OK;

	D3D12_CLEAR_VALUE clear = {};
	clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	clear.Color[0] = 0.49804f;
	clear.Color[1] = 0.78431f;
	clear.Color[2] = 0.94510f;
	clear.Color[3] = 1.0f;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = m_width;
	desc.Height = m_height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = m_msaaCount;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	HRESULT hr = m_device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clear, IID_PPV_ARGS(&m_sceneColor));
	if (FAILED(hr))
		return hr;
	SetDebugName(m_sceneColor.Get(), "SceneColorMSAA");

	m_sceneRtvIndex = AllocRtvIndex();
	if (m_sceneRtvIndex == UINT_MAX)
		return E_FAIL;
	m_device->CreateRenderTargetView(m_sceneColor.Get(), nullptr, GetRtvCpu(m_sceneRtvIndex));
	m_ownsSceneColor = true;
	m_sceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	return S_OK;
}

HRESULT DXRender::CreateDepthResolveResources()
{
	m_resolvedDepth.Reset();
	m_resolvedDepthRtvIndex = UINT_MAX;
	m_resolvedDepthSrvIndex = UINT_MAX;
	m_depthMsaaSrvIndex = UINT_MAX;
	m_depthResolveRootSig.Reset();
	m_depthResolvePso.Reset();

	if (m_msaaCount <= 1)
		return S_OK;

	D3D12_CLEAR_VALUE clear = {};
	clear.Format = DXGI_FORMAT_R32_FLOAT;
	clear.Color[0] = 1.0f;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = m_width;
	desc.Height = m_height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	HRESULT hr = m_device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clear, IID_PPV_ARGS(&m_resolvedDepth));
	if (FAILED(hr))
		return hr;
	SetDebugName(m_resolvedDepth.Get(), "ResolvedDepth");
	m_resolvedDepthState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	m_resolvedDepthRtvIndex = AllocRtvIndex();
	if (m_resolvedDepthRtvIndex == UINT_MAX)
		return E_FAIL;
	m_device->CreateRenderTargetView(
		m_resolvedDepth.Get(), nullptr, GetRtvCpu(m_resolvedDepthRtvIndex));

	m_resolvedDepthSrvIndex = CreateTextureSrv(m_resolvedDepth.Get(), DXGI_FORMAT_R32_FLOAT);
	if (m_resolvedDepthSrvIndex == UINT_MAX)
		return E_FAIL;

	D3D12_SHADER_RESOURCE_VIEW_DESC msaaSrv = {};
	msaaSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	msaaSrv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	msaaSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	m_depthMsaaSrvIndex = AllocSrvIndex();
	if (m_depthMsaaSrvIndex == UINT_MAX)
		return E_FAIL;
	m_device->CreateShaderResourceView(m_depth.Get(), &msaaSrv, GetSrvCpu(m_depthMsaaSrvIndex));

	D3D12_DESCRIPTOR_RANGE srvRange = {};
	srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange.NumDescriptors = 1;
	srvRange.BaseShaderRegister = 0;

	D3D12_ROOT_PARAMETER param = {};
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.DescriptorTable.NumDescriptorRanges = 1;
	param.DescriptorTable.pDescriptorRanges = &srvRange;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 1;
	rsDesc.pParameters = &param;
	ID3DBlob* sigBlob = nullptr;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, nullptr);
	if (FAILED(hr))
		return hr;
	hr = m_device->CreateRootSignature(
		0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_depthResolveRootSig));
	sigBlob->Release();
	if (FAILED(hr))
		return hr;

	char resolvePS[512];
	sprintf_s(resolvePS,
		"Texture2DMS<float, %u> DepthMS : register(t0);\n"
		"float4 main(float4 pos : SV_POSITION) : SV_TARGET {\n"
		"  return DepthMS.Load(int2(pos.xy), 0).r;\n"
		"}\n",
		m_msaaCount);

	static const char* resolveVS =
		"struct VSOut { float4 pos : SV_POSITION; };\n"
		"VSOut main(uint id : SV_VertexID) {\n"
		"  VSOut o;\n"
		"  float2 uv = float2((id << 1) & 2, id & 2);\n"
		"  o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);\n"
		"  return o;\n"
		"}\n";

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* err = nullptr;
	hr = D3DCompile(resolveVS, strlen(resolveVS), nullptr, nullptr, nullptr,
		"main", "vs_5_0", 0, 0, &vsBlob, &err);
	if (FAILED(hr)) {
		if (err) {
			printf("Error: depth resolve VS: %s\n", (char*)err->GetBufferPointer());
			err->Release();
		}
		return hr;
	}
	hr = D3DCompile(resolvePS, strlen(resolvePS), nullptr, nullptr, nullptr,
		"main", "ps_5_0", 0, 0, &psBlob, &err);
	if (FAILED(hr)) {
		vsBlob->Release();
		if (err) {
			printf("Error: depth resolve PS: %s\n", (char*)err->GetBufferPointer());
			err->Release();
		}
		return hr;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
	pso.pRootSignature = m_depthResolveRootSig.Get();
	pso.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	pso.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pso.SampleMask = UINT_MAX;
	pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pso.DepthStencilState.DepthEnable = FALSE;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	pso.SampleDesc.Count = 1;
	hr = m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_depthResolvePso));
	vsBlob->Release();
	psBlob->Release();
	return hr;
}

HRESULT DXRender::CreateDepth()
{
	D3D12_CLEAR_VALUE clear = {};
	clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clear.DepthStencil.Depth = 1.0f;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = m_width;
	desc.Height = m_height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	desc.SampleDesc.Count = m_msaaCount;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	HRESULT hr = m_device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clear, IID_PPV_ARGS(&m_depth));
	if (FAILED(hr))
		return hr;
	SetDebugName(m_depth.Get(), "SceneDepth");

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsv.ViewDimension = (m_msaaCount > 1)
		? D3D12_DSV_DIMENSION_TEXTURE2DMS
		: D3D12_DSV_DIMENSION_TEXTURE2D;
	m_depthDsvIndex = AllocDsvIndex();
	m_device->CreateDepthStencilView(m_depth.Get(), &dsv, GetDsvCpu(m_depthDsvIndex));

	m_depthSrvIndex = UINT_MAX;
	if (m_msaaCount <= 1) {
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Texture2D.MipLevels = 1;
		m_depthSrvIndex = AllocSrvIndex();
		m_device->CreateShaderResourceView(m_depth.Get(), &srv, GetSrvCpu(m_depthSrvIndex));
	}
	m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	return S_OK;
}

HRESULT DXRender::CreateUploadRing()
{
	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = kUploadRingSize * kFrameCount;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = m_device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&m_uploadRing));
	if (FAILED(hr))
		return hr;

	hr = m_uploadRing->Map(0, nullptr, (void**)&m_uploadMapped);
	return hr;
}

HRESULT DXRender::CreateDefaultBuffer(const void* data, UINT64 size, ID3D12Resource** outResource)
{
	if (!outResource || size == 0)
		return E_INVALIDARG;
	*outResource = nullptr;

	D3D12_HEAP_PROPERTIES defaultHeap = {};
	defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> buffer;
	HRESULT hr = m_device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&buffer));
	if (FAILED(hr))
		return hr;

	if (data) {
		D3D12_HEAP_PROPERTIES uploadHeap = {};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		ComPtr<ID3D12Resource> upload;
		hr = m_device->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&upload));
		if (FAILED(hr))
			return hr;

		void* mapped = nullptr;
		upload->Map(0, nullptr, &mapped);
		memcpy(mapped, data, (size_t)size);
		upload->Unmap(0, nullptr);

		if (!m_uploadOpen) {
			m_uploadAllocator->Reset();
			m_uploadList->Reset(m_uploadAllocator.Get(), nullptr);
			m_uploadOpen = true;
		}
		m_uploadList->CopyBufferRegion(buffer.Get(), 0, upload.Get(), 0, size);
		D3D12_RESOURCE_BARRIER b = {};
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Transition.pResource = buffer.Get();
		b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		b.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_uploadList->ResourceBarrier(1, &b);
		m_uploadKeepAlive.push_back(upload);
		/* Dest must outlive Close() even if the owner Releases early. */
		m_uploadKeepAlive.push_back(buffer);
	}

	*outResource = buffer.Detach();
	return S_OK;
}

HRESULT DXRender::CreateTexture2D(
	UINT width, UINT height, DXGI_FORMAT format,
	D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState,
	ID3D12Resource** outResource, UINT arraySize, UINT sampleCount)
{
	if (!outResource)
		return E_INVALIDARG;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = (UINT16)arraySize;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = sampleCount;
	desc.Flags = flags;

	D3D12_CLEAR_VALUE clearStorage = {};
	D3D12_CLEAR_VALUE* clear = nullptr;
	if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
		clearStorage.Format = format;
		clear = &clearStorage;
	} else if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
		clearStorage.Format = format;
		if (format == DXGI_FORMAT_R24G8_TYPELESS || format == DXGI_FORMAT_D24_UNORM_S8_UINT)
			clearStorage.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		else if (format == DXGI_FORMAT_R32_TYPELESS || format == DXGI_FORMAT_D32_FLOAT)
			clearStorage.Format = DXGI_FORMAT_D32_FLOAT;
		clearStorage.DepthStencil.Depth = 1.0f;
		clear = &clearStorage;
	}

	return m_device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, clear, IID_PPV_ARGS(outResource));
}

HRESULT DXRender::UploadTexture2D(
	ID3D12Resource* dest, const void* data, UINT rowPitch, UINT height, DXGI_FORMAT format)
{
	(void)format;
	if (!dest || !data)
		return E_INVALIDARG;

	D3D12_RESOURCE_DESC desc = dest->GetDesc();
	UINT64 required = 0;
	m_device->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &required);

	D3D12_HEAP_PROPERTIES uploadHeap = {};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = required;
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> upload;
	HRESULT hr = m_device->CreateCommittedResource(
		&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&upload));
	if (FAILED(hr))
		return hr;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
	UINT numRows = 0;
	UINT64 rowSize = 0;
	UINT64 total = 0;
	m_device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, &numRows, &rowSize, &total);

	UINT8* mapped = nullptr;
	upload->Map(0, nullptr, (void**)&mapped);
	const UINT8* src = (const UINT8*)data;
	for (UINT y = 0; y < height && y < numRows; y++) {
		memcpy(mapped + layout.Offset + y * layout.Footprint.RowPitch,
			src + y * rowPitch, (rowPitch < (UINT)rowSize) ? rowPitch : (UINT)rowSize);
	}
	upload->Unmap(0, nullptr);

	if (!m_uploadOpen) {
		m_uploadAllocator->Reset();
		m_uploadList->Reset(m_uploadAllocator.Get(), nullptr);
		m_uploadOpen = true;
	}

	D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
	dstLoc.pResource = dest;
	dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstLoc.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
	srcLoc.pResource = upload.Get();
	srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLoc.PlacedFootprint = layout;

	m_uploadList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
	m_uploadKeepAlive.push_back(upload);
	{
		ComPtr<ID3D12Resource> destKeep;
		dest->AddRef();
		destKeep.Attach(dest);
		m_uploadKeepAlive.push_back(destKeep);
	}
	return S_OK;
}

HRESULT DXRender::CreateGpuTextureFromDdsMemory(const void* ddsData, size_t ddsSize, GpuTexture* outTex)
{
	if (!ddsData || !outTex || ddsSize == 0)
		return E_INVALIDARG;
	outTex->resource = nullptr;
	outTex->srvIndex = UINT_MAX;

	TexMetadata meta;
	ScratchImage image;
	HRESULT hr = LoadFromDDSMemory((const uint8_t*)ddsData, ddsSize, DDS_FLAGS_NONE, &meta, image);
	if (FAILED(hr)) {
		printf("[Error] LoadFromDDSMemory failed (0x%08X, %zu bytes)\n", (unsigned)hr, ddsSize);
		return hr;
	}

	/* Flatten exotic TXD layouts to a single 2D mip chain (VC ground/props). */
	if (meta.IsCubemap() || meta.dimension == TEX_DIMENSION_TEXTURE3D || meta.arraySize > 1) {
		ScratchImage flattened;
		hr = flattened.Initialize2D(meta.format, meta.width, meta.height, 1, meta.mipLevels);
		if (FAILED(hr))
			return hr;
		for (size_t mip = 0; mip < meta.mipLevels; mip++) {
			const Image* src = image.GetImage(mip, 0, 0);
			const Image* dst = flattened.GetImage(mip, 0, 0);
			if (!src || !dst)
				return E_FAIL;
			memcpy(dst->pixels, src->pixels, src->slicePitch);
		}
		image = std::move(flattened);
		meta = image.GetMetadata();
	}

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = (UINT64)meta.width;
	desc.Height = (UINT)meta.height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = (UINT16)meta.mipLevels;
	desc.Format = meta.format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	ComPtr<ID3D12Resource> tex;
	hr = m_device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&tex));
	if (FAILED(hr))
		return hr;

	const UINT numSubresources = (UINT)meta.mipLevels;
	if (numSubresources == 0 || image.GetImageCount() < numSubresources)
		return E_FAIL;

	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numSubresources);
	std::vector<UINT> numRows(numSubresources);
	std::vector<UINT64> rowSizes(numSubresources);
	UINT64 totalBytes = 0;
	m_device->GetCopyableFootprints(&desc, 0, numSubresources, 0,
		layouts.data(), numRows.data(), rowSizes.data(), &totalBytes);

	D3D12_HEAP_PROPERTIES uploadHeap = {};
	uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = totalBytes;
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> upload;
	hr = m_device->CreateCommittedResource(
		&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&upload));
	if (FAILED(hr))
		return hr;

	UINT8* mapped = nullptr;
	upload->Map(0, nullptr, (void**)&mapped);
	memset(mapped, 0, (size_t)totalBytes);

	for (UINT mip = 0; mip < numSubresources; mip++) {
		const Image* img = image.GetImage(mip, 0, 0);
		if (!img)
			return E_FAIL;
		const UINT rows = numRows[mip];
		const size_t copyCols = (size_t)((rowSizes[mip] < img->rowPitch) ? rowSizes[mip] : img->rowPitch);
		for (UINT y = 0; y < rows; y++) {
			if ((size_t)y * img->rowPitch + copyCols > img->slicePitch)
				break;
			memcpy(
				mapped + layouts[mip].Offset + (UINT64)y * layouts[mip].Footprint.RowPitch,
				img->pixels + (size_t)y * img->rowPitch,
				copyCols);
		}
	}
	upload->Unmap(0, nullptr);

	if (!m_uploadOpen) {
		m_uploadAllocator->Reset();
		m_uploadList->Reset(m_uploadAllocator.Get(), nullptr);
		m_uploadOpen = true;
	}

	for (UINT mip = 0; mip < numSubresources; mip++) {
		D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
		dstLoc.pResource = tex.Get();
		dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLoc.SubresourceIndex = mip;

		D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
		srcLoc.pResource = upload.Get();
		srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLoc.PlacedFootprint = layouts[mip];
		m_uploadList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
	}

	D3D12_RESOURCE_BARRIER b = {};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = tex.Get();
	b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_uploadList->ResourceBarrier(1, &b);
	m_uploadKeepAlive.push_back(upload);
	m_uploadKeepAlive.push_back(tex);

	outTex->srvIndex = CreateTextureSrv(tex.Get(), meta.format);
	if (outTex->srvIndex == UINT_MAX) {
		printf("[Error] CreateGpuTextureFromDdsMemory: SRV alloc failed (used %u)\n", m_srvCount);
		return E_FAIL;
	}
	outTex->resource = tex.Detach();
	return S_OK;
}

HRESULT DXRender::CreateGpuTextureFromPixels(
	const void* pixels, UINT width, UINT height, UINT rowPitch,
	DXGI_FORMAT format, GpuTexture* outTex)
{
	if (!pixels || !outTex || width == 0 || height == 0 || rowPitch == 0)
		return E_INVALIDARG;
	outTex->resource = nullptr;
	outTex->srvIndex = UINT_MAX;

	ID3D12Resource* tex = nullptr;
	HRESULT hr = CreateTexture2D(
		width, height, format, D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST, &tex);
	if (FAILED(hr))
		return hr;

	hr = UploadTexture2D(tex, pixels, rowPitch, height, format);
	if (FAILED(hr)) {
		tex->Release();
		return hr;
	}

	/* Transition to shader resource on the upload list. */
	if (!m_uploadOpen) {
		m_uploadAllocator->Reset();
		m_uploadList->Reset(m_uploadAllocator.Get(), nullptr);
		m_uploadOpen = true;
	}
	D3D12_RESOURCE_BARRIER b = {};
	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = tex;
	b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_uploadList->ResourceBarrier(1, &b);

	outTex->srvIndex = CreateTextureSrv(tex, format);
	if (outTex->srvIndex == UINT_MAX) {
		tex->Release();
		return E_FAIL;
	}
	outTex->resource = tex;
	return S_OK;
}

void DXRender::FlushUploads()
{
	if (!m_uploadOpen)
		return;
	m_uploadList->Close();
	m_uploadOpen = false;
	ID3D12CommandList* lists[] = { m_uploadList.Get() };
	m_queue->ExecuteCommandLists(1, lists);
	WaitForGpu();
	m_uploadKeepAlive.clear();
}

void* DXRender::AllocFrameConstants(UINT64 size, D3D12_GPU_VIRTUAL_ADDRESS* outGpuAddress)
{
	UINT64 aligned = (size + 255ull) & ~255ull;
	FrameContext& frame = m_frames[m_frameIndex];

	/* Prefer the per-frame ring slice. Never wrap — that corrupts in-flight CBs. */
	if (frame.uploadOffset + aligned <= kUploadRingSize) {
		UINT64 offset = (UINT64)m_frameIndex * kUploadRingSize + frame.uploadOffset;
		frame.uploadOffset += aligned;
		if (outGpuAddress)
			*outGpuAddress = m_uploadRing->GetGPUVirtualAddress() + offset;
		return m_uploadMapped + offset;
	}

	/* Overflow: dedicated UPLOAD chunk for the rest of this frame. */
	if (!frame.overflowMapped || frame.overflowOffset + aligned > frame.overflowSize) {
		const UINT64 chunk = (std::max)(aligned, (UINT64)(4 * 1024 * 1024));
		D3D12_HEAP_PROPERTIES heap = {};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = chunk;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		ComPtr<ID3D12Resource> buf;
		if (FAILED(m_device->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&buf)))) {
			printf("[Error] AllocFrameConstants: overflow upload failed\n");
			if (outGpuAddress)
				*outGpuAddress = 0;
			return nullptr;
		}

		void* mapped = nullptr;
		buf->Map(0, nullptr, &mapped);
		frame.overflowMapped = (UINT8*)mapped;
		frame.overflowSize = chunk;
		frame.overflowOffset = 0;
		frame.overflowGpuBase = buf->GetGPUVirtualAddress();
		frame.overflowUploads.push_back(buf);

		static bool s_logged = false;
		if (!s_logged) {
			printf("[Warn] Frame constant ring full (%llu MB) — using overflow uploads\n",
				(unsigned long long)(kUploadRingSize / (1024 * 1024)));
			s_logged = true;
		}
	}

	UINT64 off = frame.overflowOffset;
	frame.overflowOffset += aligned;
	if (outGpuAddress)
		*outGpuAddress = frame.overflowGpuBase + off;
	return frame.overflowMapped + off;
}

void DXRender::RestoreMainTargets()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetSceneRtv();
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDsv();
	if (m_ownsSceneColor && m_sceneColor &&
		m_sceneColorState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		Transition(m_sceneColor.Get(), m_sceneColorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_sceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	if (m_depthState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		Transition(m_depth.Get(), m_depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
	D3D12_VIEWPORT vp = MakeViewport();
	D3D12_RECT sc = MakeScissor();
	m_commandList->RSSetViewports(1, &vp);
	m_commandList->RSSetScissorRects(1, &sc);
}

void DXRender::BindColorTargetOnly()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetSceneRtv();
	if (m_ownsSceneColor && m_sceneColor &&
		m_sceneColorState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		Transition(m_sceneColor.Get(), m_sceneColorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_sceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	D3D12_VIEWPORT vp = MakeViewport();
	D3D12_RECT sc = MakeScissor();
	m_commandList->RSSetViewports(1, &vp);
	m_commandList->RSSetScissorRects(1, &sc);
}

void DXRender::BindBackBufferOnly()
{
	if (m_backBufferState[m_frameIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		Transition(m_renderTargets[m_frameIndex].Get(),
			m_backBufferState[m_frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_backBufferState[m_frameIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetBackBufferRtv();
	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	D3D12_VIEWPORT vp = MakeViewport();
	D3D12_RECT sc = MakeScissor();
	m_commandList->RSSetViewports(1, &vp);
	m_commandList->RSSetScissorRects(1, &sc);
}

void DXRender::ResolveMSAA()
{
	if (!m_ownsSceneColor || m_msaaCount <= 1 || !m_sceneColor)
		return;

	if (m_sceneColorState != D3D12_RESOURCE_STATE_RESOLVE_SOURCE) {
		Transition(m_sceneColor.Get(), m_sceneColorState, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		m_sceneColorState = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	}
	if (m_backBufferState[m_frameIndex] != D3D12_RESOURCE_STATE_RESOLVE_DEST) {
		Transition(m_renderTargets[m_frameIndex].Get(),
			m_backBufferState[m_frameIndex], D3D12_RESOURCE_STATE_RESOLVE_DEST);
		m_backBufferState[m_frameIndex] = D3D12_RESOURCE_STATE_RESOLVE_DEST;
	}

	m_commandList->ResolveSubresource(
		m_renderTargets[m_frameIndex].Get(), 0,
		m_sceneColor.Get(), 0,
		DXGI_FORMAT_R8G8B8A8_UNORM);

	Transition(m_renderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_backBufferState[m_frameIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void DXRender::ResolveDepthForSSAO()
{
	if (m_msaaCount <= 1 || !m_depthResolvePso || m_depthMsaaSrvIndex == UINT_MAX) {
		/* Single-sample depth: just transition to SRV for sampling. */
		if (m_depthState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
			Transition(m_depth.Get(), m_depthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_depthState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
		return;
	}

	if (m_depthState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
		Transition(m_depth.Get(), m_depthState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_depthState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
	if (m_resolvedDepthState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		Transition(m_resolvedDepth.Get(), m_resolvedDepthState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_resolvedDepthState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRtvCpu(m_resolvedDepthRtvIndex);
	m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	D3D12_VIEWPORT vp = MakeViewport();
	D3D12_RECT sc = MakeScissor();
	m_commandList->RSSetViewports(1, &vp);
	m_commandList->RSSetScissorRects(1, &sc);

	BindDescriptorHeaps();
	m_commandList->SetGraphicsRootSignature(m_depthResolveRootSig.Get());
	m_commandList->SetPipelineState(m_depthResolvePso.Get());
	m_commandList->SetGraphicsRootDescriptorTable(0, GetSrvGpu(m_depthMsaaSrvIndex));
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	Transition(m_resolvedDepth.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_resolvedDepthState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

HRESULT DXRender::Init(HWND hWnd, bool vsync)
{
	m_vsync = vsync;
	m_hWnd = hWnd;
	m_width = 0;
	m_height = 0;
	m_rtvCount = 0;
	m_dsvCount = 0;
	m_srvCount = 0;
	m_samplerCount = 0;
	m_rasterMode = RasterCullMode::CullFront;
	m_solidRasterMode = RasterCullMode::CullFront;
	m_blendMode = BlendPassMode::Opaque;

	RECT rc;
	GetClientRect(hWnd, &rc);
	m_width = rc.right - rc.left;
	m_height = rc.bottom - rc.top;

	HRESULT hr = CreateDevice();
	if (FAILED(hr))
		return hr;
	hr = CreateHeaps();
	if (FAILED(hr))
		return hr;
	hr = CreateSwapChain(hWnd);
	if (FAILED(hr))
		return hr;
	hr = CreateFrameResources();
	if (FAILED(hr))
		return hr;

	m_msaaCount = PickMSAACount(DXGI_FORMAT_R8G8B8A8_UNORM);
	hr = CreateMSAAColor();
	if (FAILED(hr)) {
		printf("Error: cannot CreateMSAAColor\n");
		return hr;
	}
	hr = CreateDepth();
	if (FAILED(hr))
		return hr;
	hr = CreateDepthResolveResources();
	if (FAILED(hr)) {
		printf("Error: cannot CreateDepthResolveResources\n");
		return hr;
	}
	hr = CreateUploadRing();
	if (FAILED(hr))
		return hr;

	printf("[Info] DX12 FL 12_0, %ux%u, MSAA %ux\n", m_width, m_height, m_msaaCount);
	if (SupportsRaytracing()) {
		HRESULT rtHr = EnsureFallbackTlas();
		if (FAILED(rtHr))
			printf("[Warn] Fallback empty TLAS failed (0x%08X)\n", (unsigned)rtHr);
		else
			printf("[Info] Fallback empty TLAS ready (VA=0x%llX)\n",
				(unsigned long long)m_fallbackTlasVA);
	}
	return S_OK;
}

HRESULT DXRender::EnsureFallbackTlas()
{
	if (m_fallbackTlasVA != 0)
		return S_OK;
	if (!m_device5)
		return E_FAIL;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = 0;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = 0;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
	m_device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);
	if (prebuild.ResultDataMaxSizeInBytes == 0)
		return E_FAIL;

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC asDesc = {};
	asDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	asDesc.Width = prebuild.ResultDataMaxSizeInBytes;
	asDesc.Height = 1;
	asDesc.DepthOrArraySize = 1;
	asDesc.MipLevels = 1;
	asDesc.SampleDesc.Count = 1;
	asDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	asDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = m_device5->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &asDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr, IID_PPV_ARGS(&m_fallbackTlas));
	if (FAILED(hr))
		return hr;

	ComPtr<ID3D12Resource> scratch;
	if (prebuild.ScratchDataSizeInBytes > 0) {
		D3D12_RESOURCE_DESC scratchDesc = asDesc;
		scratchDesc.Width = prebuild.ScratchDataSizeInBytes;
		hr = m_device5->CreateCommittedResource(
			&heap, D3D12_HEAP_FLAG_NONE, &scratchDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr, IID_PPV_ARGS(&scratch));
		if (FAILED(hr))
			return hr;
	}

	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList4> list4;
	hr = m_device5->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
	if (FAILED(hr))
		return hr;
	{
		ComPtr<ID3D12GraphicsCommandList> list;
		hr = m_device5->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&list));
		if (FAILED(hr))
			return hr;
		hr = list.As(&list4);
		if (FAILED(hr))
			return hr;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build = {};
	build.Inputs = inputs;
	build.DestAccelerationStructureData = m_fallbackTlas->GetGPUVirtualAddress();
	build.ScratchAccelerationStructureData = scratch ? scratch->GetGPUVirtualAddress() : 0;
	list4->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
	list4->Close();

	ID3D12CommandList* lists[] = { list4.Get() };
	m_queue->ExecuteCommandLists(1, lists);
	WaitForGpu();

	m_fallbackTlasVA = m_fallbackTlas->GetGPUVirtualAddress();
	return S_OK;
}

void DXRender::Cleanup()
{
	WaitForGpu();
	ProcessDeferredReleases(UINT64_MAX);

	if (m_uploadMapped && m_uploadRing) {
		m_uploadRing->Unmap(0, nullptr);
		m_uploadMapped = nullptr;
	}

	m_uploadKeepAlive.clear();
	m_uploadList.Reset();
	m_uploadAllocator.Reset();
	m_uploadRing.Reset();
	m_commandList.Reset();

	for (UINT i = 0; i < kFrameCount; i++) {
		m_frames[i].overflowUploads.clear();
		m_frames[i].overflowMapped = nullptr;
		m_frames[i].allocator.Reset();
		m_renderTargets[i].Reset();
	}
	m_sceneColor.Reset();
	m_ownsSceneColor = false;
	m_sceneRtvIndex = UINT_MAX;
	m_resolvedDepth.Reset();
	m_depthResolvePso.Reset();
	m_depthResolveRootSig.Reset();
	m_depth.Reset();
	m_msaaCount = 1;
	m_fallbackTlas.Reset();
	m_fallbackTlasVA = 0;
	m_srvHeap.Reset();
	m_samplerHeap.Reset();
	m_rtvHeap.Reset();
	m_dsvHeap.Reset();
	m_swapChain.Reset();
	m_queue.Reset();
	m_fence.Reset();
	if (m_fenceEvent) {
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
	m_device.Reset();
	m_device5.Reset();
	m_raytracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

void DXRender::MoveToNextFrame()
{
	FrameContext& frame = m_frames[m_frameIndex];
	frame.fenceValue = ++m_fenceValue;
	m_queue->Signal(m_fence.Get(), frame.fenceValue);

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	FrameContext& next = m_frames[m_frameIndex];
	if (m_fence->GetCompletedValue() < next.fenceValue) {
		m_fence->SetEventOnCompletion(next.fenceValue, m_fenceEvent);
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
	ProcessDeferredReleases(m_fence->GetCompletedValue());
	next.uploadOffset = 0;
	next.overflowUploads.clear();
	next.overflowMapped = nullptr;
	next.overflowSize = 0;
	next.overflowOffset = 0;
	next.overflowGpuBase = 0;
}

void DXRender::RenderStart()
{
	FlushUploads();

	FrameContext& frame = m_frames[m_frameIndex];
	frame.allocator->Reset();
	m_commandList->Reset(frame.allocator.Get(), nullptr);
	BindDescriptorHeaps();

	if (m_ownsSceneColor && m_sceneColor) {
		if (m_sceneColorState != D3D12_RESOURCE_STATE_RENDER_TARGET) {
			Transition(m_sceneColor.Get(), m_sceneColorState, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_sceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
	} else if (m_backBufferState[m_frameIndex] != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		Transition(m_renderTargets[m_frameIndex].Get(),
			m_backBufferState[m_frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_backBufferState[m_frameIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	if (m_depthState != D3D12_RESOURCE_STATE_DEPTH_WRITE) {
		Transition(m_depth.Get(), m_depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	const float clearColor[4] = { 0.49804f, 0.78431f, 0.94510f, 1.0f };
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetSceneRtv();
	m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(GetDsv(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	RestoreMainTargets();
	SetOpaqueState();
}

void DXRender::RenderEnd()
{
	if (m_backBufferState[m_frameIndex] != D3D12_RESOURCE_STATE_PRESENT) {
		Transition(m_renderTargets[m_frameIndex].Get(),
			m_backBufferState[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT);
		m_backBufferState[m_frameIndex] = D3D12_RESOURCE_STATE_PRESENT;
	}

	m_commandList->Close();
	ID3D12CommandList* lists[] = { m_commandList.Get() };
	m_queue->ExecuteCommandLists(1, lists);
	HRESULT presentHr = m_swapChain->Present(m_vsync ? 1 : 0, 0);
	if (FAILED(presentHr)) {
		HRESULT removed = m_device ? m_device->GetDeviceRemovedReason() : presentHr;
		printf("[Error] Present failed (0x%08X) deviceRemoved=0x%08X\n",
			(unsigned)presentHr, (unsigned)removed);
		fflush(stdout);
#if defined(_DEBUG)
		ComPtr<ID3D12InfoQueue> iq;
		if (m_device && SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&iq)))) {
			const UINT64 n = iq->GetNumStoredMessages();
			UINT64 start = (n > 16) ? (n - 16) : 0;
			for (UINT64 i = start; i < n; i++) {
				SIZE_T bytes = 0;
				iq->GetMessage(i, nullptr, &bytes);
				if (bytes == 0)
					continue;
				std::vector<char> buf(bytes);
				auto* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
				if (SUCCEEDED(iq->GetMessage(i, msg, &bytes)) && msg->pDescription)
					printf("[D3D12] %s\n", msg->pDescription);
			}
			fflush(stdout);
		}
#endif
	}
	MoveToNextFrame();
}
