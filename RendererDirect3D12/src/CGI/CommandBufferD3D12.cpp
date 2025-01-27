// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

CommandBufferD3D12::CommandBufferD3D12()
{
}

CommandBufferD3D12::~CommandBufferD3D12()
{
	Done();
}

void CommandBufferD3D12::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_D3D12;

	_vClearValues.reserve(16);
	_vAttachmentStates.reserve(4);
	_vBarriers.reserve(VERUS_MAX_FB_ATTACH);
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_pCommandLists[i] = pRendererD3D12->CreateD3DCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, pRendererD3D12->GetD3DCommandAllocator(i));
}

void CommandBufferD3D12::Done()
{
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
	{
		VERUS_COM_RELEASE_CHECK(_pCommandLists[i].Get());
		_pCommandLists[i].Reset();
	}
	VERUS_RT_ASSERT(_vAttachmentStates.capacity() < 100);
	VERUS_RT_ASSERT(_vBarriers.capacity() < 1000);

	VERUS_DONE(CommandBufferD3D12);
}

void CommandBufferD3D12::InitOneTimeSubmit()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;

	_pOneTimeCommandAllocator = pRendererD3D12->CreateD3DCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto pCmdList = pRendererD3D12->CreateD3DCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, _pOneTimeCommandAllocator);
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_pCommandLists[i] = pCmdList;
	_pOneTimeCommandAllocator->Reset();
	Begin();
	pRendererD3D12->SetDescriptorHeaps(this);
}

void CommandBufferD3D12::DoneOneTimeSubmit()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;

	End();
	ID3D12CommandList* ppCommandLists[] = { _pCommandLists[0].Get() };
	pRendererD3D12->GetD3DCommandQueue()->ExecuteCommandLists(VERUS_COUNT_OF(ppCommandLists), ppCommandLists);
	pRendererD3D12->QueueWaitIdle();
	_pOneTimeCommandAllocator.Reset();
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_pCommandLists[i].Reset();
}

void CommandBufferD3D12::Begin()
{
	VERUS_QREF_RENDERER_D3D12;

	HRESULT hr = 0;
	if (FAILED(hr = GetD3DGraphicsCommandList()->Reset(
		_pOneTimeCommandAllocator ? _pOneTimeCommandAllocator.Get() : pRendererD3D12->GetD3DCommandAllocator(pRendererD3D12->GetRingBufferIndex()),
		nullptr)))
		throw VERUS_RUNTIME_ERROR << "Reset(); hr=" << VERUS_HR(hr);
}

void CommandBufferD3D12::End()
{
	HRESULT hr = 0;
	if (FAILED(hr = GetD3DGraphicsCommandList()->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(); hr=" << VERUS_HR(hr);
}

void CommandBufferD3D12::PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range mipLevels, Range arrayLayers)
{
	auto& texD3D12 = static_cast<RTextureD3D12>(*tex);
	_vBarriers.clear();
	_vBarriers.reserve(mipLevels.GetCount() * arrayLayers.GetCount());
	for (int layer : arrayLayers)
	{
		for (int mip : mipLevels)
		{
			const UINT subresource = D3D12CalcSubresource(mip, layer, 0, texD3D12.GetMipLevelCount(), texD3D12.GetArrayLayerCount());
			_vBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				texD3D12.GetD3DResource(), ToNativeImageLayout(oldLayout), ToNativeImageLayout(newLayout), subresource));
		}
	}
	if (!_vBarriers.empty())
		GetD3DGraphicsCommandList()->ResourceBarrier(static_cast<UINT>(_vBarriers.size()), _vBarriers.data());
}

void CommandBufferD3D12::BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle, std::initializer_list<Vector4> ilClearValues, ViewportScissorFlags vsf)
{
	VERUS_QREF_RENDERER_D3D12;

	_pRenderPass = &pRendererD3D12->GetRenderPass(renderPassHandle);
	_pFramebuffer = &pRendererD3D12->GetFramebuffer(framebufferHandle);

	VERUS_RT_ASSERT(_pRenderPass->_vAttachments.size() == ilClearValues.size());

	_vClearValues.clear();
	for (const auto& x : ilClearValues)
	{
		auto pColor = x.ToPointer();
		_vClearValues.push_back(pColor[0]);
		_vClearValues.push_back(pColor[1]);
		_vClearValues.push_back(pColor[2]);
		_vClearValues.push_back(pColor[3]);
	}
	_vAttachmentStates.clear();
	for (const auto& attachment : _pRenderPass->_vAttachments)
		_vAttachmentStates.push_back(attachment._initialState);

	_subpassIndex = 0;
	PrepareSubpass();

	SetViewportAndScissor(vsf, _pFramebuffer->_width, _pFramebuffer->_height);
}

void CommandBufferD3D12::NextSubpass()
{
	_subpassIndex++;
	PrepareSubpass();
}

void CommandBufferD3D12::EndRenderPass()
{
	auto pCmdList = GetD3DGraphicsCommandList();

	_vBarriers.clear();
	_vBarriers.reserve(VERUS_MAX_FB_ATTACH);
	int index = 0;
	for (const auto& attachment : _pRenderPass->_vAttachments)
	{
		if (_vAttachmentStates[index] != attachment._finalState)
		{
			const auto& resource = _pFramebuffer->_vResources[index];
			UINT subresource = 0;
			if (CubeMapFace::none != _pFramebuffer->_cubeMapFace)
			{
				UINT arrayLayer = 0;
				switch (_pFramebuffer->_cubeMapFace)
				{
				case CubeMapFace::all: arrayLayer = ToNativeCubeMapFace(static_cast<CubeMapFace>(index)); break;
				default:               arrayLayer = !index ? ToNativeCubeMapFace(_pFramebuffer->_cubeMapFace) : 0;
				}
				subresource = D3D12CalcSubresource(0, arrayLayer, 0, _pFramebuffer->_mipLevels, 0);
			}
			_vBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, _vAttachmentStates[index], attachment._finalState, subresource));
		}
		index++;
	}
	if (!_vBarriers.empty())
		pCmdList->ResourceBarrier(static_cast<UINT>(_vBarriers.size()), _vBarriers.data());

	_pRenderPass = nullptr;
	_pFramebuffer = nullptr;
	_subpassIndex = 0;
}

void CommandBufferD3D12::BindPipeline(PipelinePtr pipe)
{
	auto pCmdList = GetD3DGraphicsCommandList();
	auto& pipeD3D12 = static_cast<RPipelineD3D12>(*pipe);
	pCmdList->SetPipelineState(pipeD3D12.GetD3DPipelineState());
	if (pipeD3D12.IsCompute())
	{
		pCmdList->SetComputeRootSignature(pipeD3D12.GetD3DRootSignature());
	}
	else
	{
		pCmdList->SetGraphicsRootSignature(pipeD3D12.GetD3DRootSignature());
		pCmdList->IASetPrimitiveTopology(pipeD3D12.GetD3DPrimitiveTopology());
	}
}

void CommandBufferD3D12::SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth)
{
	if (il.size() > 0)
	{
		const float w = il.begin()->Width();
		const float h = il.begin()->Height();
		_viewportSize = Vector4(w, h, 1 / w, 1 / h);
	}

	VERUS_RT_ASSERT(il.size() <= VERUS_MAX_CA);
	CD3DX12_VIEWPORT vpD3D12[VERUS_MAX_CA];
	UINT count = 0;
	for (const auto& rc : il)
		vpD3D12[count++] = CD3DX12_VIEWPORT(rc.getX(), rc.getY(), rc.Width(), rc.Height(), minDepth, maxDepth);
	GetD3DGraphicsCommandList()->RSSetViewports(count, vpD3D12);
}

void CommandBufferD3D12::SetScissor(std::initializer_list<Vector4> il)
{
	VERUS_RT_ASSERT(il.size() <= VERUS_MAX_CA);
	CD3DX12_RECT rcD3D12[VERUS_MAX_CA];
	UINT count = 0;
	for (const auto& rc : il)
		rcD3D12[count++] = CD3DX12_RECT(
			static_cast<LONG>(rc.Left()),
			static_cast<LONG>(rc.Top()),
			static_cast<LONG>(rc.Right()),
			static_cast<LONG>(rc.Bottom()));
	GetD3DGraphicsCommandList()->RSSetScissorRects(count, rcD3D12);
}

void CommandBufferD3D12::SetBlendConstants(const float* p)
{
	GetD3DGraphicsCommandList()->OMSetBlendFactor(p);
}

void CommandBufferD3D12::BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter)
{
	auto& geoD3D12 = static_cast<RGeometryD3D12>(*geo);
	D3D12_VERTEX_BUFFER_VIEW views[VERUS_MAX_VB];
	const int count = geoD3D12.GetVertexBufferCount();
	int filteredCount = 0;
	VERUS_FOR(i, count)
	{
		if ((bindingsFilter >> i) & 0x1)
		{
			VERUS_RT_ASSERT(filteredCount < VERUS_MAX_VB);
			views[filteredCount] = *geoD3D12.GetD3DVertexBufferView(i);
			filteredCount++;
		}
	}
	GetD3DGraphicsCommandList()->IASetVertexBuffers(0, filteredCount, views);
}

void CommandBufferD3D12::BindIndexBuffer(GeometryPtr geo)
{
	auto& geoD3D12 = static_cast<RGeometryD3D12>(*geo);
	GetD3DGraphicsCommandList()->IASetIndexBuffer(geoD3D12.GetD3DIndexBufferView());
}

bool CommandBufferD3D12::BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle)
{
	auto& shaderD3D12 = static_cast<RShaderD3D12>(*shader);
	if (shaderD3D12.TryRootConstants(setNumber, *this))
		return true;

	const D3D12_GPU_DESCRIPTOR_HANDLE hGPU = shaderD3D12.UpdateUniformBuffer(setNumber, complexSetHandle.Get());
	if (!hGPU.ptr)
		return false;

	auto pCmdList = GetD3DGraphicsCommandList();
	const UINT rootParameterIndex = shaderD3D12.ToRootParameterIndex(setNumber);
	if (shaderD3D12.IsCompute())
		pCmdList->SetComputeRootDescriptorTable(rootParameterIndex, hGPU);
	else
		pCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, hGPU);

	if (complexSetHandle.IsSet() && !shaderD3D12.IsCompute())
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE hGPU = shaderD3D12.UpdateSamplers(setNumber, complexSetHandle.Get());
		if (hGPU.ptr)
		{
			const UINT rootParameterIndex = shaderD3D12.GetDescriptorSetCount();
			pCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, hGPU);
		}
	}

	return true;
}

void CommandBufferD3D12::PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags)
{
	auto& shaderD3D12 = static_cast<RShaderD3D12>(*shader);
	if (shaderD3D12.IsCompute())
		GetD3DGraphicsCommandList()->SetComputeRoot32BitConstants(0, size, p, offset);
	else
		GetD3DGraphicsCommandList()->SetGraphicsRoot32BitConstants(0, size, p, offset);
}

void CommandBufferD3D12::Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
	GetD3DGraphicsCommandList()->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferD3D12::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
	GetD3DGraphicsCommandList()->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBufferD3D12::Dispatch(int groupCountX, int groupCountY, int groupCountZ)
{
	GetD3DGraphicsCommandList()->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandBufferD3D12::DispatchMesh(int groupCountX, int groupCountY, int groupCountZ)
{
	//GetD3DGraphicsCommandList()->DispatchMesh(groupCountX, groupCountY, groupCountZ);
}

void CommandBufferD3D12::TraceRays(int width, int height, int depth)
{
	//D3D12_DISPATCH_RAYS_DESC desc = {};
	//desc.Width = width;
	//desc.Height = height;
	//desc.Depth = depth;
	//GetD3DGraphicsCommandList()->DispatchRays(&desc);
}

ID3D12GraphicsCommandList3* CommandBufferD3D12::GetD3DGraphicsCommandList() const
{
	VERUS_QREF_RENDERER;
	return _pCommandLists[renderer->GetRingBufferIndex()].Get();
}

void CommandBufferD3D12::PrepareSubpass()
{
	auto pCmdList = GetD3DGraphicsCommandList();

	RP::RcD3DSubpass subpass = _pRenderPass->_vSubpasses[_subpassIndex];
	RP::RcD3DFramebufferSubpass fs = _pFramebuffer->_vSubpasses[_subpassIndex];

	// Resource transitions for this subpass:
	_vBarriers.clear();
	_vBarriers.reserve(VERUS_MAX_FB_ATTACH);
	int index = 0;
	auto CalcSubresource = [this](int index) -> UINT
	{
		if (CubeMapFace::none == _pFramebuffer->_cubeMapFace)
		{
			return 0;
		}
		else
		{
			UINT arrayLayer = 0;
			switch (_pFramebuffer->_cubeMapFace)
			{
			case CubeMapFace::all: arrayLayer = ToNativeCubeMapFace(static_cast<CubeMapFace>(index)); break;
			default:               arrayLayer = !index ? ToNativeCubeMapFace(_pFramebuffer->_cubeMapFace) : 0;
			}
			return D3D12CalcSubresource(0, arrayLayer, 0, _pFramebuffer->_mipLevels, 0);
		}
	};
	VERUS_FOR(i, subpass._vInput.size())
	{
		const auto& ref = subpass._vInput[i];
		if (_vAttachmentStates[ref._index] != ref._state)
		{
			const auto& resource = fs._vResources[index];
			_vBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, _vAttachmentStates[ref._index], ref._state, CalcSubresource(index)));
			_vAttachmentStates[ref._index] = ref._state;
		}
		index++;
	}
	VERUS_FOR(i, subpass._vColor.size())
	{
		const auto& ref = subpass._vColor[i];
		if (_vAttachmentStates[ref._index] != ref._state)
		{
			const auto& resource = fs._vResources[index];
			_vBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, _vAttachmentStates[ref._index], ref._state, CalcSubresource(index)));
			_vAttachmentStates[ref._index] = ref._state;
		}
		index++;
	}
	if (subpass._depthStencil._index >= 0)
	{
		const auto& ref = subpass._depthStencil;
		if (_vAttachmentStates[ref._index] != ref._state)
		{
			_vBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(fs._vResources.back(), _vAttachmentStates[ref._index], ref._state, CalcSubresource(index)));
			_vAttachmentStates[ref._index] = ref._state;
		}
	}
	if (!_vBarriers.empty())
		pCmdList->ResourceBarrier(static_cast<UINT>(_vBarriers.size()), _vBarriers.data());

	// Clear attachments for this subpass:
	index = 0;
	for (const auto& ref : subpass._vColor)
	{
		const auto& attachment = _pRenderPass->_vAttachments[ref._index];
		if (RP::Attachment::LoadOp::clear == attachment._loadOp && attachment._clearSubpassIndex == _subpassIndex)
			pCmdList->ClearRenderTargetView(fs._dhRTVs.AtCPU(index), &_vClearValues[ref._index << 2], 0, nullptr);
		index++;
	}
	if (subpass._depthStencil._index >= 0)
	{
		const auto& attachment = _pRenderPass->_vAttachments[subpass._depthStencil._index];
		if (RP::Attachment::LoadOp::clear == attachment._loadOp && attachment._clearSubpassIndex == _subpassIndex)
		{
			D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
			UINT8 stencil = 0;
			if (RP::Attachment::LoadOp::clear == attachment._stencilLoadOp)
			{
				clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
				stencil = static_cast<UINT8>(_vClearValues[(subpass._depthStencil._index << 2) + 1]);
			}
			pCmdList->ClearDepthStencilView(fs._dhDSV.AtCPU(0), clearFlags, _vClearValues[subpass._depthStencil._index << 2], stencil, 0, nullptr);
		}
	}

	auto hRTV = fs._dhRTVs.AtCPU(0);
	auto hDSV = fs._dhDSV.AtCPU(0);
	pCmdList->OMSetRenderTargets(
		Utils::Cast32(subpass._vColor.size()),
		fs._dhRTVs.GetD3DDescriptorHeap() ? &hRTV : nullptr,
		TRUE,
		fs._dhDSV.GetD3DDescriptorHeap() ? &hDSV : nullptr);
}
