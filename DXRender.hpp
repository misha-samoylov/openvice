#pragma once

#include <stdio.h>
#include <vector>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

using Microsoft::WRL::ComPtr;

/* CPU-side texture handle: DEFAULT resource + SRV index in the shared heap. */
struct GpuTexture
{
	ID3D12Resource* resource = nullptr;
	UINT srvIndex = UINT_MAX;

	bool Valid() const { return resource != nullptr && srvIndex != UINT_MAX; }
};

enum class RasterCullMode
{
	CullFront,
	CullNone,
	Wireframe
};

enum class BlendPassMode
{
	Opaque,
	Cutout,
	SoftAlpha
};

class DXRender
{
public:
	static const UINT kFrameCount = 3;
	/* VC map meshes allocate one SRV each; 4k fills quickly. */
	static const UINT kSrvHeapSize = 65536;
	static const UINT kRtvHeapSize = 64;
	static const UINT kDsvHeapSize = 32;
	static const UINT kSamplerHeapSize = 64;
	static const UINT64 kUploadRingSize = 64 * 1024 * 1024;

	HRESULT Init(HWND hWnd, bool vsync);
	void Cleanup();

	void RenderStart();
	void RenderEnd();

	void RestoreMainTargets();
	void BindColorTargetOnly();
	void BindBackBufferOnly();
	void ResolveMSAA();
	void ResolveDepthForSSAO();

	void SetOpaqueState();
	void SetCutoutAlphaState();
	void SetSoftAlphaState();
	BlendPassMode GetBlendPassMode() const { return m_blendMode; }

	ID3D12Device* GetDevice() { return m_device.Get(); }
	ID3D12Device5* GetDevice5() { return m_device5.Get(); }
	bool SupportsRaytracing() const { return m_raytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED; }
	ID3D12GraphicsCommandList* GetCommandList() { return m_commandList.Get(); }
	ID3D12CommandQueue* GetQueue() { return m_queue.Get(); }

	UINT GetDepthSrvIndex() const;
	ID3D12Resource* GetDepthResource() const { return m_depth.Get(); }
	ID3D12Resource* GetBackBuffer() const { return m_renderTargets[m_frameIndex].Get(); }
	ID3D12Resource* GetSceneColor() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRtv() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSceneRtv() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;

	UINT GetMSAASampleCount() const { return m_msaaCount; }

	HRESULT ChangeRasterizerStateToWireframe();
	HRESULT ChangeRasterizerStateToSolid();
	void ApplyRasterizerState();
	void SetCullNone();
	void SetCullFront();
	void SetVehicleRasterizer(bool wireframe);
	bool IsWireframe() const { return m_rasterMode == RasterCullMode::Wireframe; }
	RasterCullMode GetRasterCullMode() const { return m_rasterMode; }

	UINT GetBackBufferWidth() const { return m_width; }
	UINT GetBackBufferHeight() const { return m_height; }
	UINT GetSrvCount() const { return m_srvCount; }

	/* Descriptor heaps */
	UINT AllocSrvIndex();
	UINT CreateTextureSrv(ID3D12Resource* resource, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
	UINT CreateTexture2DArraySrv(ID3D12Resource* resource, DXGI_FORMAT format, UINT arraySize);
	UINT CreateTypedBufferSrv(ID3D12Resource* resource, UINT elementCount, UINT stride);
	UINT CreateAccelerationStructureSrv(ID3D12Resource* accelerationStructure);
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpu(UINT index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu(UINT index) const;

	UINT CreateSampler(const D3D12_SAMPLER_DESC& desc);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpu(UINT index) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpu(UINT index) const;

	UINT AllocRtvIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE CreateRtv(ID3D12Resource* resource, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCpu(UINT index) const;

	UINT AllocDsvIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE CreateDsv(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpu(UINT index) const;

	ID3D12DescriptorHeap* GetSrvHeap() const { return m_srvHeap.Get(); }
	ID3D12DescriptorHeap* GetSamplerHeap() const { return m_samplerHeap.Get(); }

	/* Resource helpers — upload + GPU wait (safe during Init). */
	HRESULT CreateDefaultBuffer(const void* data, UINT64 size, ID3D12Resource** outResource);
	HRESULT CreateTexture2D(
		UINT width, UINT height, DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState,
		ID3D12Resource** outResource, UINT arraySize = 1, UINT sampleCount = 1);
	HRESULT UploadTexture2D(
		ID3D12Resource* dest, const void* data, UINT rowPitch, UINT height,
		DXGI_FORMAT format);
	HRESULT CreateGpuTextureFromDdsMemory(const void* ddsData, size_t ddsSize, GpuTexture* outTex);
	HRESULT CreateGpuTextureFromPixels(
		const void* pixels, UINT width, UINT height, UINT rowPitch,
		DXGI_FORMAT format, GpuTexture* outTex);

	/* Per-frame upload ring (no GPU wait; valid until RenderEnd of this frame). */
	void* AllocFrameConstants(UINT64 size, D3D12_GPU_VIRTUAL_ADDRESS* outGpuAddress);

	void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void DeferRelease(IUnknown* obj);
	void WaitForGpu();
	void FlushUploads(); /* execute pending init uploads and wait */

	void BindDescriptorHeaps();

	/* Empty TLAS for safe RayQuery binds when scene AS is not ready. */
	HRESULT EnsureFallbackTlas();
	D3D12_GPU_VIRTUAL_ADDRESS GetFallbackTlasVA() const { return m_fallbackTlasVA; }

private:
	static constexpr UINT kDesiredMSAA = 4;

	struct FrameContext
	{
		ComPtr<ID3D12CommandAllocator> allocator;
		UINT64 fenceValue = 0;
		UINT64 uploadOffset = 0;
		/* Extra UPLOAD buffers when the per-frame ring fills (never wrap mid-frame). */
		std::vector<ComPtr<ID3D12Resource>> overflowUploads;
		UINT8* overflowMapped = nullptr;
		UINT64 overflowSize = 0;
		UINT64 overflowOffset = 0;
		D3D12_GPU_VIRTUAL_ADDRESS overflowGpuBase = 0;
	};

	HRESULT CreateDevice();
	HRESULT CreateSwapChain(HWND hWnd);
	HRESULT CreateHeaps();
	HRESULT CreateFrameResources();
	HRESULT CreateDepth();
	HRESULT CreateMSAAColor();
	HRESULT CreateDepthResolveResources();
	UINT PickMSAACount(DXGI_FORMAT format);
	HRESULT CreateUploadRing();
	void MoveToNextFrame();
	void ProcessDeferredReleases(UINT64 completedFence);
	D3D12_VIEWPORT MakeViewport() const;
	D3D12_RECT MakeScissor() const;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Device5> m_device5;
	D3D12_RAYTRACING_TIER m_raytracingTier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	ComPtr<ID3D12Resource> m_fallbackTlas;
	D3D12_GPU_VIRTUAL_ADDRESS m_fallbackTlasVA = 0;
	ComPtr<ID3D12CommandQueue> m_queue;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent = nullptr;
	UINT64 m_fenceValue = 0;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
	UINT m_rtvDescriptorSize = 0;
	UINT m_dsvDescriptorSize = 0;
	UINT m_srvDescriptorSize = 0;
	UINT m_samplerDescriptorSize = 0;
	UINT m_rtvCount = 0;
	UINT m_dsvCount = 0;
	UINT m_srvCount = 0;
	UINT m_samplerCount = 0;

	ComPtr<ID3D12Resource> m_renderTargets[kFrameCount];
	UINT m_backBufferRtvIndices[kFrameCount] = {};
	/* MSAA offscreen color (null / unused when m_msaaCount == 1). */
	ComPtr<ID3D12Resource> m_sceneColor;
	UINT m_sceneRtvIndex = UINT_MAX;
	D3D12_RESOURCE_STATES m_sceneColorState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	bool m_ownsSceneColor = false;

	ComPtr<ID3D12Resource> m_depth;
	UINT m_depthDsvIndex = 0;
	UINT m_depthSrvIndex = UINT_MAX; /* single-sample depth SRV (or resolved) */
	UINT m_depthMsaaSrvIndex = UINT_MAX;
	ComPtr<ID3D12Resource> m_resolvedDepth;
	UINT m_resolvedDepthRtvIndex = UINT_MAX;
	UINT m_resolvedDepthSrvIndex = UINT_MAX;
	ComPtr<ID3D12RootSignature> m_depthResolveRootSig;
	ComPtr<ID3D12PipelineState> m_depthResolvePso;
	D3D12_RESOURCE_STATES m_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	D3D12_RESOURCE_STATES m_resolvedDepthState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	D3D12_RESOURCE_STATES m_backBufferState[kFrameCount] = {};
	UINT m_msaaCount = 1;

	FrameContext m_frames[kFrameCount];
	UINT m_frameIndex = 0;

	ComPtr<ID3D12Resource> m_uploadRing;
	UINT8* m_uploadMapped = nullptr;

	/* Init-time copy list (separate from frame list). */
	ComPtr<ID3D12CommandAllocator> m_uploadAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_uploadList;
	bool m_uploadOpen = false;
	std::vector<ComPtr<ID3D12Resource>> m_uploadKeepAlive;

	struct DeferredRelease
	{
		UINT64 fenceValue;
		IUnknown* obj;
	};
	std::vector<DeferredRelease> m_deferred;

	UINT m_width = 0;
	UINT m_height = 0;
	bool m_vsync = false;
	HWND m_hWnd = nullptr;

	RasterCullMode m_rasterMode = RasterCullMode::CullFront;
	RasterCullMode m_solidRasterMode = RasterCullMode::CullFront;
	BlendPassMode m_blendMode = BlendPassMode::Opaque;
};
