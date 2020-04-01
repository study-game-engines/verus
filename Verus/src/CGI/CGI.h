#pragma once

#include "Types.h"
#include "Formats.h"
#include "RenderPass.h"
#include "Scheduled.h"

#include "BaseGeometry.h"
#include "BaseTexture.h"
#include "BaseShader.h"
#include "BasePipeline.h"
#include "BaseCommandBuffer.h"
#include "BaseRenderer.h"

#include "DebugDraw.h"
#include "DeferredShading.h"
#include "Renderer.h"
#include "RendererParser.h"

namespace verus
{
	void Make_CGI();
	void Free_CGI();
}
