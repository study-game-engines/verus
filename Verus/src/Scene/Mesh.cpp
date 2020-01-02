#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn                    Mesh::s_shader;
CGI::PipelinePwns<Mesh::PIPE_MAX> Mesh::s_pipe;

Mesh::UB_PerFrame      Mesh::s_ubPerFrame;
Mesh::UB_PerMaterialFS Mesh::s_ubPerMaterialFS;
Mesh::UB_PerMeshVS     Mesh::s_ubPerMeshVS;
Mesh::UB_SkeletonVS    Mesh::s_ubSkeletonVS;
Mesh::UB_PerObject     Mesh::s_ubPerObject;

// Mesh:

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
	Done();
}

void Mesh::InitStatic()
{
	VERUS_QREF_CONST_SETTINGS;

	s_shader.Init("[Shaders]:DS_Mesh.hlsl");
	s_shader->CreateDescriptorSet(0, &s_ubPerFrame, sizeof(s_ubPerFrame), settings.GetLimits()._mesh_ubPerFrameCapacity);
	s_shader->CreateDescriptorSet(1, &s_ubPerMaterialFS, sizeof(s_ubPerMaterialFS), settings.GetLimits()._mesh_ubPerMaterialFSCapacity,
		{
			CGI::Sampler::aniso,
			CGI::Sampler::aniso
		}, CGI::ShaderStageFlags::fs);
	s_shader->CreateDescriptorSet(2, &s_ubPerMeshVS, sizeof(s_ubPerMeshVS), settings.GetLimits()._mesh_ubPerMeshVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader->CreateDescriptorSet(3, &s_ubSkeletonVS, sizeof(s_ubSkeletonVS), settings.GetLimits()._mesh_ubSkinningVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader->CreateDescriptorSet(4, &s_ubPerObject, sizeof(s_ubPerObject), 0);
	s_shader->CreatePipelineLayout();
}

void Mesh::DoneStatic()
{
	s_pipe.Done();
	s_shader.Done();
}

void Mesh::Init(RcDesc desc)
{
	VERUS_RT_ASSERT(desc._url);

	_instanceCapacity = desc._instanceCapacity;

	BaseMesh::Init(desc._url);
}

void Mesh::Done()
{
	VERUS_DONE(Mesh);
}

void Mesh::BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb)
{
	VERUS_QREF_RENDERER;
	if (!s_pipe[pipe])
	{
		static CSZ branches[] =
		{
			"#",
			"#DepthRobotic",
			"#DepthSkinned",
			"#Instanced",
			"#Robotic",
			"#Skinned"
		};
		CGI::PipelineDesc pipeDesc(_geo, s_shader, branches[pipe], renderer.GetDS().GetRenderPassID());
		switch (pipe)
		{
		case PIPE_DEPTH_ROBOTIC:
		case PIPE_DEPTH_SKINNED:
			pipeDesc._colorAttachBlendEqs[0] = "";
			pipeDesc.EnableDepthBias();
		default:
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
			pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
			pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		}
		pipeDesc._vertexInputBindingsFilter = _bindingsMask;
		s_pipe[pipe].Init(pipeDesc);
	}
	cb->BindPipeline(s_pipe[pipe]);
}

void Mesh::BindGeo(CGI::CommandBufferPtr cb)
{
	BindGeo(cb, _bindingsMask);
}

void Mesh::BindGeo(CGI::CommandBufferPtr cb, UINT32 bindingsFilter)
{
	cb->BindVertexBuffers(_geo, bindingsFilter);
	cb->BindIndexBuffer(_geo);
}

void Mesh::UpdateUniformBufferPerFrame()
{
	VERUS_QREF_SM;
	s_ubPerFrame._matV = sm.GetCamera()->GetMatrixV().UniformBufferFormat();
	s_ubPerFrame._matP = sm.GetCamera()->GetMatrixP().UniformBufferFormat();
	s_ubPerFrame._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
}

void Mesh::UpdateUniformBufferPerMaterialFS()
{
}

void Mesh::UpdateUniformBufferPerMeshVS()
{
	memcpy(&s_ubPerMeshVS._posDeqScale, _posDeq + 0, 12);
	memcpy(&s_ubPerMeshVS._posDeqBias, _posDeq + 3, 12);
	memcpy(&s_ubPerMeshVS._tc0DeqScaleBias, _tc0Deq, 16);
	memcpy(&s_ubPerMeshVS._tc1DeqScaleBias, _tc1Deq, 16);
}

void Mesh::UpdateUniformBufferSkeletonVS()
{
	_skeleton.UpdateUniformBufferArray(s_ubSkeletonVS._vMatBones);
}

void Mesh::UpdateUniformBufferPerObject(RcTransform3 tr)
{
	s_ubPerObject._matW = tr.UniformBufferFormat();
	s_ubPerObject._userColor = Vector4(1, 0, 0, 1).GLM();
}

void Mesh::UpdateUniformBufferPerObject(Point3 pos)
{
	const Transform3 matT = Transform3::translation(Vector3(pos));
	s_ubPerObject._matW = matT.UniformBufferFormat();
	s_ubPerObject._userColor = Vector4(1, 0, 0, 1).GLM();
}

void Mesh::CreateDeviceBuffers()
{
	_bindingsMask = 0;

	CGI::GeometryDesc geoDesc;
	const CGI::InputElementDesc ied[] =
	{
		{0, offsetof(VertexInputBinding0, _pos), CGI::IeType::shorts, 4, CGI::IeUsage::position, 0},
		{0, offsetof(VertexInputBinding0, _tc0), CGI::IeType::shorts, 2, CGI::IeUsage::texCoord, 0},
		{0, offsetof(VertexInputBinding0, _nrm), CGI::IeType::ubytes, 4, CGI::IeUsage::normal,   0},
		{1, offsetof(VertexInputBinding1, _bw),  CGI::IeType::shorts, 4, CGI::IeUsage::texCoord, 4},
		{1, offsetof(VertexInputBinding1, _bi),  CGI::IeType::shorts, 4, CGI::IeUsage::texCoord, 5},
		{2, offsetof(VertexInputBinding2, _tan), CGI::IeType::shorts, 4, CGI::IeUsage::tangent,  6},
		{2, offsetof(VertexInputBinding2, _bin), CGI::IeType::shorts, 4, CGI::IeUsage::binormal, 7},
		{3, offsetof(VertexInputBinding3, _tc1), CGI::IeType::shorts, 2, CGI::IeUsage::texCoord, 1},
		{3, offsetof(VertexInputBinding3, _clr), CGI::IeType::ubytes, 4, CGI::IeUsage::color,    0},

		{-4, offsetof(PerInstanceData, _matPart0), CGI::IeType::floats, 4, CGI::IeUsage::texCoord, 8},
		{-4, offsetof(PerInstanceData, _matPart1), CGI::IeType::floats, 4, CGI::IeUsage::texCoord, 9},
		{-4, offsetof(PerInstanceData, _matPart2), CGI::IeType::floats, 4, CGI::IeUsage::texCoord, 10},
		{-4, offsetof(PerInstanceData, _instData), CGI::IeType::floats, 4, CGI::IeUsage::texCoord, 11},
		CGI::InputElementDesc::End()
	};
	geoDesc._pInputElementDesc = ied;
	const int strides[] = { sizeof(VertexInputBinding0), sizeof(VertexInputBinding1), sizeof(VertexInputBinding2), sizeof(VertexInputBinding3), sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	geoDesc._32BitIndices = _vIndices.empty();
	_geo.Init(geoDesc);

	// Vertex buffer, binding 0:
	_bindingsMask |= (1 << 0);
	_geo->CreateVertexBuffer(Utils::Cast32(_vBinding0.size()), 0);
	_geo->UpdateVertexBuffer(_vBinding0.data(), 0);

	// Vertex buffer, binding 1 (skinning):
	if (!_vBinding1.empty())
	{
		_bindingsMask |= (1 << 1);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding1.size()), 1);
		_geo->UpdateVertexBuffer(_vBinding1.data(), 1);
	}

	// Vertex buffer, binding 2 (tangent space):
	if (!_vBinding2.empty())
	{
		_bindingsMask |= (1 << 2);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding2.size()), 2);
		_geo->UpdateVertexBuffer(_vBinding2.data(), 2);
	}

	// Vertex buffer, binding 3 (lightmap):
	if (!_vBinding3.empty())
	{
		_bindingsMask |= (1 << 3);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding3.size()), 3);
		_geo->UpdateVertexBuffer(_vBinding3.data(), 3);
	}

	// Index buffer:
	if (!_vIndices.empty())
	{
		_geo->CreateIndexBuffer(Utils::Cast32(_vIndices.size()));
		_geo->UpdateIndexBuffer(_vIndices.data());
	}
	else if (!_vIndices32.empty())
	{
		_geo->CreateIndexBuffer(Utils::Cast32(_vIndices32.size()));
		_geo->UpdateIndexBuffer(_vIndices32.data());
	}

	// Instance buffer:
	if (_instanceCapacity > 1)
	{
		_vInstanceBuffer.resize(_instanceCapacity);
		_bindingsMask |= (1 << 4);
		_geo->CreateVertexBuffer(Utils::Cast32(_vInstanceBuffer.size()), 4);
	}
}

void Mesh::BufferDataVB(const void* p, int binding)
{
	_geo->UpdateVertexBuffer(p, binding);
}

void Mesh::PushInstance(RcTransform3 matW, RcVector4 instData)
{
	if (!_vertCount)
		return;
	if (IsInstanceBufferFull())
		return;
	matW.InstFormat(&_vInstanceBuffer[_instanceCount]._matPart0);
	_vInstanceBuffer[_instanceCount]._instData = instData;
	_instanceCount++;
}

bool Mesh::IsInstanceBufferFull()
{
	if (!_vertCount)
		return false;
	return _instanceCount >= _instanceCapacity;
}

bool Mesh::IsInstanceBufferEmpty()
{
	if (!_vertCount)
		return true;
	return _instanceCount <= 0;
}

void Mesh::ResetNumInstances()
{
	_instanceCount = 0;
}

void Mesh::UpdateInstanceBuffer()
{
	_geo->UpdateVertexBuffer(_vInstanceBuffer.data(), 4);
}
