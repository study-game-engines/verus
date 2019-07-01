#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

// DescriptorHeap:

void DescriptorHeap::Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num, bool shaderVisible)
{
	HRESULT hr = 0;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = _type = type;
	desc.NumDescriptors = num;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_pDescriptorHeap))))
		throw VERUS_RUNTIME_ERROR << "CreateDescriptorHeap(), hr=" << VERUS_HR(hr);
	_hCPUHandleForHeapStart = _pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	_hGPUHandleForHeapStart = _pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	_handleIncrementSize = pDevice->GetDescriptorHandleIncrementSize(desc.Type);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AtCPU(INT index)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(_hCPUHandleForHeapStart, index, _handleIncrementSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::AtGPU(INT index)
{
	VERUS_RT_ASSERT(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == _type || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == _type);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(_hGPUHandleForHeapStart, index, _handleIncrementSize);
}

// DynamicDescriptorHeap:

void DynamicDescriptorHeap::Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num, bool shaderVisible)
{
	_capacity = num;
	_offset = 0;
	DescriptorHeap::Create(pDevice, type, _capacity * BaseRenderer::s_ringBufferSize, shaderVisible);
}

std::pair<CD3DX12_CPU_DESCRIPTOR_HANDLE, CD3DX12_GPU_DESCRIPTOR_HANDLE> DynamicDescriptorHeap::GetNextHandle()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;

	if (_currentFrame != renderer.GetNumFrames())
	{
		_currentFrame = renderer.GetNumFrames();
		_offset = 0;
	}

	if (_offset < _capacity)
	{
		const INT index = _capacity * pRendererD3D12->GetRingBufferIndex() + _offset;
		_offset++;
		return std::make_pair(AtCPU(index), AtGPU(index));
	}
	return std::make_pair(CD3DX12_CPU_DESCRIPTOR_HANDLE(), CD3DX12_GPU_DESCRIPTOR_HANDLE());
}