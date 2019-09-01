#include "Lib.hlsl"
#include "GenerateMips.inc.hlsl"

ConstantBuffer<UB_GenerateMips> g_ub : register(b0, space0);

Texture2D    g_texSrcMip : register(t1, space0);
SamplerState g_samSrcMip : register(s1, space0);

RWTexture2D<float4> g_uavOutMip1 : register(u2, space0);
RWTexture2D<float4> g_uavOutMip2 : register(u3, space0);
RWTexture2D<float4> g_uavOutMip3 : register(u4, space0);
RWTexture2D<float4> g_uavOutMip4 : register(u5, space0);

struct CSI
{
	uint3 _DispatchThreadID : SV_DispatchThreadID;
	uint  _GroupIndex       : SV_GroupIndex;
};

#define THREAD_GROUP_SIZE 8

#define DIM_CASE_WE_HE 0
#define DIM_CASE_WO_HE 1
#define DIM_CASE_WE_HO 2
#define DIM_CASE_WO_HO 3

groupshared float gs_R[64];
groupshared float gs_G[64];
groupshared float gs_B[64];
groupshared float gs_A[64];

void StoreColor(uint index, float4 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
	gs_A[index] = color.a;
}

float4 LoadColor(uint index)
{
	return float4(gs_R[index], gs_G[index], gs_B[index], gs_A[index]);
}

float3 ConvertToLinear(float3 x)
{
	return x < 0.04045f ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

float3 ConvertToSRGB(float3 x)
{
	return x < 0.0031308 ? 12.92 * x : 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
}

float4 PackColor(float4 x)
{
	if (g_ub._isSRGB)
	{
		return float4(ConvertToSRGB(x.rgb), x.a);
	}
	else
	{
		return x;
	}
}

#ifdef _CS
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void mainCS(CSI si)
{
	float4 srcColor1 = 0.0;

	switch (g_ub._srcDimensionCase)
	{
	case DIM_CASE_WE_HE:
	{
		const float2 tc = g_ub._texelSize * (si._DispatchThreadID.xy + 0.5);

		srcColor1 = g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel);
	}
	break;
	case DIM_CASE_WO_HE:
	{
		const float2 tc = g_ub._texelSize * (si._DispatchThreadID.xy + float2(0.25, 0.5));
		const float2 offset = g_ub._texelSize * float2(0.5, 0.0);

		srcColor1 = lerp(
			g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel),
			g_texSrcMip.SampleLevel(g_samSrcMip, tc + offset, g_ub._srcMipLevel),
			0.5);
	}
	break;
	case DIM_CASE_WE_HO:
	{
		const float2 tc = g_ub._texelSize * (si._DispatchThreadID.xy + float2(0.5, 0.25));
		const float2 offset = g_ub._texelSize * float2(0.0, 0.5);

		srcColor1 = lerp(
			g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel),
			g_texSrcMip.SampleLevel(g_samSrcMip, tc + offset, g_ub._srcMipLevel),
			0.5);
	}
	break;
	case DIM_CASE_WO_HO:
	{
		const float2 tc = g_ub._texelSize * (si._DispatchThreadID.xy + float2(0.25, 0.25));
		const float2 offset = g_ub._texelSize * 0.5;

		srcColor1 = lerp(
			lerp(
				g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel),
				g_texSrcMip.SampleLevel(g_samSrcMip, tc + float2(offset.x, 0.0), g_ub._srcMipLevel),
				0.5),
			lerp(g_texSrcMip.SampleLevel(g_samSrcMip, tc + float2(0.0, offset.y), g_ub._srcMipLevel),
				g_texSrcMip.SampleLevel(g_samSrcMip, tc + float2(offset.x, offset.y), g_ub._srcMipLevel),
				0.5),
			0.5);
	}
	break;
	}

	g_uavOutMip1[si._DispatchThreadID.xy] = PackColor(srcColor1);

	if (1 == g_ub._numMipLevels)
		return;

	StoreColor(si._GroupIndex, srcColor1);

	GroupMemoryBarrierWithGroupSync();

	if ((si._GroupIndex & 0x9) == 0) // 16 threads:
	{
		const float4 srcColor2 = LoadColor(si._GroupIndex + 0x01); // {+0, +1}
		const float4 srcColor3 = LoadColor(si._GroupIndex + 0x08); // {+1, +0}
		const float4 srcColor4 = LoadColor(si._GroupIndex + 0x09); // {+1, +1}
		srcColor1 = 0.25 * (srcColor1 + srcColor2 + srcColor3 + srcColor4);

		g_uavOutMip2[si._DispatchThreadID.xy >> 1] = PackColor(srcColor1);
		StoreColor(si._GroupIndex, srcColor1);
	}

	if (2 == g_ub._numMipLevels)
		return;

	GroupMemoryBarrierWithGroupSync();

	if ((si._GroupIndex & 0x1B) == 0) // 4 threads:
	{
		const float4 srcColor2 = LoadColor(si._GroupIndex + 0x02); // {+0, +2}
		const float4 srcColor3 = LoadColor(si._GroupIndex + 0x10); // {+2, +0}
		const float4 srcColor4 = LoadColor(si._GroupIndex + 0x12); // {+2, +2}
		srcColor1 = 0.25 * (srcColor1 + srcColor2 + srcColor3 + srcColor4);

		g_uavOutMip3[si._DispatchThreadID.xy >> 2] = PackColor(srcColor1);
		StoreColor(si._GroupIndex, srcColor1);
	}

	if (3 == g_ub._numMipLevels)
		return;

	GroupMemoryBarrierWithGroupSync();

	if (si._GroupIndex == 0) // 1 thread:
	{
		const float4 srcColor2 = LoadColor(si._GroupIndex + 0x04); // {+0, +4}
		const float4 srcColor3 = LoadColor(si._GroupIndex + 0x20); // {+4, +0}
		const float4 srcColor4 = LoadColor(si._GroupIndex + 0x24); // {+4, +4}
		srcColor1 = 0.25 * (srcColor1 + srcColor2 + srcColor3 + srcColor4);

		g_uavOutMip4[si._DispatchThreadID.xy >> 3] = PackColor(srcColor1);
	}
}
#endif

//@main:T
