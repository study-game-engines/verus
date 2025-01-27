// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "DS_Forest.inc.hlsl"

CBUFFER(0, UB_ForestVS, g_ubForestVS)
CBUFFER(1, UB_ForestFS, g_ubForestFS)

Texture2D    g_texGBuffer0 : REG(t1, space1, t0);
SamplerState g_samGBuffer0 : REG(s1, space1, s0);
Texture2D    g_texGBuffer1 : REG(t2, space1, t1);
SamplerState g_samGBuffer1 : REG(s2, space1, s1);
Texture2D    g_texGBuffer2 : REG(t3, space1, t2);
SamplerState g_samGBuffer2 : REG(s3, space1, s2);
Texture2D    g_texGBuffer3 : REG(t4, space1, t3);
SamplerState g_samGBuffer3 : REG(s4, space1, s3);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
	VK_LOCATION(8)       float2 tc0 : TEXCOORD0; // {psize, angle}
};

struct VSO
{
	float4 pos    : SV_Position;
	float2 tc0    : TEXCOORD0;
	float2 angles : TEXCOORD1;
	float4 color  : COLOR0;
	float2 psize  : PSIZE;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float pointSpriteSize = si.tc0.x;
	const float angle = si.tc0.y;

	const float3 toEye = g_ubForestVS._eyePos.xyz - si.pos.xyz;
	const float distToHead = distance(g_ubForestVS._headPos.xyz, si.pos.xyz);

	const float nearAlpha = ComputeFade(distToHead, 60.0, 90.0);
	const float farAlpha = 1.0 - ComputeFade(distToHead, 900.0, 1000.0);

	so.pos = mul(si.pos, g_ubForestVS._matWVP);
	so.tc0 = 0.0;
	so.color.rgb = RandomColor(si.pos.xz, 0.25, 0.15);
	so.color.a = nearAlpha * farAlpha;
	so.psize = pointSpriteSize * (g_ubForestVS._viewportSize.yx * g_ubForestVS._viewportSize.z) * g_ubForestVS._matP._m11;
	so.psize *= ceil(so.color.a); // Clipping.

	float2 paramA = toEye.xy;
	float2 paramB = toEye.zz;
	paramA.y = max(0.0, paramA.y); // Only upper hemisphere.
	paramB.y = length(toEye.xz); // Distance in XZ-plane.
	so.angles = (atan2(paramA, paramB) + _PI) * (0.5 / _PI); // atan2(x, z) and atan2(max(0, y), length(toEye.xz)). From 0 to 1.
	so.angles.y = (so.angles.y - 0.5) * 4.0; // Choose this quadrant.
	so.angles = saturate(so.angles);
	so.angles.x = frac(so.angles.x - angle + 0.5); // Turn.

	return so;
}
#endif

#ifdef _GS
[maxvertexcount(4)]
void mainGS(point VSO si[1], inout TriangleStream<VSO> stream)
{
	VSO so;

	so = si[0];
	const float2 center = so.pos.xy;
	for (int i = 0; i < 4; ++i)
	{
		const float2 posOffset = _POINT_SPRITE_POS_OFFSETS[i] * so.psize;
		const float2 offset = float2(
			dot(posOffset, g_ubForestVS._spriteMat.xy),
			dot(posOffset, g_ubForestVS._spriteMat.zw));
		so.pos.xy = center + offset;
		so.tc0 = _POINT_SPRITE_TEX_COORDS[i];
		stream.Append(so);
	}
}
#endif

float4 ComputeTexCoords(float2 tc, float2 angles, out float frameBlend)
{
	const float margin = 4.0 / 512.0; // 4 pixels.
	const float tcScale = 1.0 - margin * 2.0;
	const float2 tcWithMargin = tc * tcScale + margin;

	const float2 frameCount = float2(16, 16);
	const float2 frameScale = 1.0 / frameCount;
	const float2 frameOffset = angles * frameCount;
	const float2 maxOffset = float2(256, frameCount.y - 0.5);
	const float4 frameBias = floor(min(frameOffset.xyxy + float4(0, 0.5, 1, 0.5), maxOffset.xyxy));
	frameBlend = frac(frameOffset.x);
	return (tcWithMargin.xyxy + frameBias) * frameScale.xyxy;
}

float ComputeMask(float2 tc, float alpha)
{
	const float2 tcCenter = tc - 0.5;
	const float grad = 1.0 - saturate(dot(tcCenter, tcCenter) * 4.0);
	return saturate(grad + (alpha * 2.0 - 1.0));
}

#ifdef _FS
#ifdef DEF_DEPTH
void mainFS(VSO si)
{
	float frameBlend;
	const float4 tc = ComputeTexCoords(si.tc0, si.angles, frameBlend);
	const float mask = ComputeMask(si.tc0, si.color.a);

	const float alpha0 = g_texGBuffer1.Sample(g_samGBuffer1, tc.xy).a;
	const float alpha1 = g_texGBuffer1.Sample(g_samGBuffer1, tc.zw).a;
	const float alpha = lerp(alpha0, alpha1, frameBlend);

	clip(alpha * mask - 0.5);
}
#else
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

	const float dither = Dither2x2(si.pos.xy);

	float frameBlend;
	const float4 tc = ComputeTexCoords(si.tc0, si.angles, frameBlend);
	const float mask = ComputeMask(si.tc0, si.color.a);

	const float4 gBuffer0Sam_F0 = g_texGBuffer0.Sample(g_samGBuffer0, tc.xy);
	const float4 gBuffer0Sam_F1 = g_texGBuffer0.Sample(g_samGBuffer0, tc.zw);
	const float4 gBuffer1Sam_F0 = float4(
		g_texGBuffer1.Sample(g_samGBuffer1, tc.xy).rgb,
		g_texGBuffer1.SampleBias(g_samGBuffer1, tc.xy, 1.0).a);
	const float4 gBuffer1Sam_F1 = float4(
		g_texGBuffer1.Sample(g_samGBuffer1, tc.zw).rgb,
		g_texGBuffer1.SampleBias(g_samGBuffer1, tc.zw, 1.0).a);
	const float4 gBuffer2Sam_F0 = g_texGBuffer2.Sample(g_samGBuffer2, tc.xy);
	const float4 gBuffer2Sam_F1 = g_texGBuffer2.Sample(g_samGBuffer2, tc.zw);
	const float4 gBuffer3Sam_F0 = g_texGBuffer3.Sample(g_samGBuffer3, tc.xy);
	const float4 gBuffer3Sam_F1 = g_texGBuffer3.Sample(g_samGBuffer3, tc.zw);

	const float alpha = lerp(gBuffer1Sam_F0.a, gBuffer1Sam_F1.a, frameBlend);
	const float2 normAlphas = float2(gBuffer1Sam_F0.a, gBuffer1Sam_F1.a) / max(max(gBuffer1Sam_F0.a, gBuffer1Sam_F1.a), _SINGULARITY_FIX);
	const float opaqueBlend = clamp(frameBlend, normAlphas.y - normAlphas.x, normAlphas.y);

	so.target0 = lerp(gBuffer0Sam_F0, gBuffer0Sam_F1, opaqueBlend);
	so.target1 = lerp(gBuffer1Sam_F0, gBuffer1Sam_F1, opaqueBlend);
	so.target2 = lerp(gBuffer2Sam_F0, gBuffer2Sam_F1, opaqueBlend);
	so.target3 = lerp(gBuffer3Sam_F0, gBuffer3Sam_F1, opaqueBlend);

	so.target0.rgb = saturate(so.target0.rgb * si.color.rgb * 2.0);
	so.target1.a = 1.0;
	so.target3.a = max(so.target3.a, AlphaToResolveDitheringMask(alpha));

	const float fudgeFactor = 1.5;
	clip(saturate(alpha * fudgeFactor) * mask - (dither + (1.0 / 8.0)));

	return so;
}
#endif
#endif

//@main:# (VGF)
//@main:#Depth DEPTH (VGF)
