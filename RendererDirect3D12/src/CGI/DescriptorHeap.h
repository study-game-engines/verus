#pragma once

namespace verus
{
	namespace CGI
	{
		class DescriptorHeap
		{
			ComPtr<ID3D12DescriptorHeap>  _pDescriptorHeap;
			CD3DX12_CPU_DESCRIPTOR_HANDLE _hCPUHandleForHeapStart;
			CD3DX12_GPU_DESCRIPTOR_HANDLE _hGPUHandleForHeapStart;
			D3D12_DESCRIPTOR_HEAP_TYPE    _type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
			UINT                          _handleIncrementSize = 0;

		public:
			void Reset() { VERUS_COM_RELEASE_CHECK(_pDescriptorHeap.Get()); _pDescriptorHeap.Reset(); }

			ID3D12DescriptorHeap* GetD3DDescriptorHeap() const { return _pDescriptorHeap.Get(); }

			void Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num, bool shaderVisible = false);

			CD3DX12_CPU_DESCRIPTOR_HANDLE AtCPU(INT index);
			CD3DX12_GPU_DESCRIPTOR_HANDLE AtGPU(INT index);
		};
		VERUS_TYPEDEFS(DescriptorHeap);

		class DynamicDescriptorHeap : public DescriptorHeap
		{
			UINT   _capacity = 0;
			UINT   _offset = 0;
			UINT64 _currentFrame = UINT64_MAX;

		public:
			void Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num, bool shaderVisible = false);

			std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, CD3DX12_GPU_DESCRIPTOR_HANDLE> GetNextHandle();

			UINT GetCapacity() const { return _capacity; }
			UINT GetOffset() const { return _offset; }
		};
		VERUS_TYPEDEFS(DynamicDescriptorHeap);
	}
}