#include "verus.h"

using namespace verus;
using namespace verus::CGI;

DebugDraw::UB_DebugDraw DebugDraw::s_ubDebugDraw;

DebugDraw::DebugDraw()
{
	VERUS_CT_ASSERT(16 == sizeof(Vertex));
}

DebugDraw::~DebugDraw()
{
	Done();
}

void DebugDraw::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_shader.Init("[Shaders]:DebugDraw.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubDebugDraw, sizeof(s_ubDebugDraw), 0);
	_shader->CreatePipelineLayout();

	GeometryDesc geoDesc;
	const InputElementDesc ied[] =
	{
		{0, offsetof(Vertex, _pos),   IeType::floats, 3, IeUsage::position, 0},
		{0, offsetof(Vertex, _color), IeType::ubytes, 4, IeUsage::color, 0},
		InputElementDesc::End()
	};
	geoDesc._pInputElementDesc = ied;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	geoDesc._dynamic = true;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(_maxNumVert, 0);

	_vDynamicBuffer.resize(_maxNumVert);
	_numVert = 0;
	_offset = 0;

	{
		PipelineDesc pipeDesc(_geo, _shader, "#", renderer.GetRenderPass_SwapChainDepth());
		pipeDesc._rasterizationState._polygonMode = PolygonMode::line;
		pipeDesc._topology = PrimitiveTopology::pointList;
		_pipe[PIPE_POINTS].Init(pipeDesc);
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_POINTS_NO_Z].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(_geo, _shader, "#", renderer.GetRenderPass_SwapChainDepth());
		pipeDesc._rasterizationState._polygonMode = PolygonMode::line;
		pipeDesc._topology = PrimitiveTopology::lineList;
		_pipe[PIPE_LINES].Init(pipeDesc);
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_LINES_NO_Z].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(_geo, _shader, "#", renderer.GetRenderPass_SwapChainDepth());
		pipeDesc._rasterizationState._polygonMode = PolygonMode::line;
		pipeDesc._topology = PrimitiveTopology::triangleList;
		_pipe[PIPE_POLY].Init(pipeDesc);
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_POLY_NO_Z].Init(pipeDesc);
	}
}

void DebugDraw::Done()
{
	VERUS_DONE(DebugDraw);
}

void DebugDraw::Begin(Type type, PcTransform3 pMat, bool zEnable)
{
	VERUS_QREF_SM;
	VERUS_QREF_RENDERER;

	_type = type;
	_numVert = 0;
	if (_currentFrame != renderer.GetNumFrames())
	{
		_currentFrame = renderer.GetNumFrames();
		_offset = 0;
	}

	PIPE pipe = PIPE_POINTS;
	switch (type)
	{
	case Type::points: pipe = PIPE_POINTS; break;
	case Type::lines: pipe = PIPE_LINES; break;
	case Type::poly: pipe = PIPE_POLY; break;
	}
	if (!zEnable)
		pipe = static_cast<PIPE>(pipe + 1);

	renderer.GetCommandBuffer()->BindPipeline(_pipe[pipe]);
	renderer.GetCommandBuffer()->BindVertexBuffers(_geo);

	Matrix4 matWVP;
	if (sm.GetCamera())
		matWVP = sm.GetCamera()->GetMatrixVP();
	else
		matWVP = Matrix4::identity();

	s_ubDebugDraw._matWVP = pMat ? Matrix4(matWVP * *pMat).UniformBufferFormat() : matWVP.UniformBufferFormat();

	_shader->BeginBindDescriptors();
	renderer.GetCommandBuffer()->BindDescriptors(_shader, 0);
	_shader->EndBindDescriptors();
}

void DebugDraw::End()
{
	VERUS_QREF_RENDERER;

	_geo->UpdateVertexBuffer(_vDynamicBuffer.data(), 0);
	renderer.GetCommandBuffer()->Draw(_numVert, 1, _offset);
	_offset = _numVert;
	_numVert = 0;
}

bool DebugDraw::AddPoint(
	RcPoint3 pos,
	UINT32 color)
{
	return false;
}

bool DebugDraw::AddLine(
	RcPoint3 posA,
	RcPoint3 posB,
	UINT32 color)
{
	const int at = _offset + _numVert;
	if (at + 2 > _maxNumVert)
		return false;
	posA.ToArray3(_vDynamicBuffer[at]._pos);
	Utils::CopyColor(_vDynamicBuffer[at]._color, color);
	posB.ToArray3(_vDynamicBuffer[at + 1]._pos);
	Utils::CopyColor(_vDynamicBuffer[at + 1]._color, color);
	_numVert += 2;
	_peakLoad = Math::Max(_peakLoad, at + 2);
	return true;
}

bool DebugDraw::AddTriangle(
	RcPoint3 posA,
	RcPoint3 posB,
	RcPoint3 posC,
	UINT32 color)
{
	return false;
}