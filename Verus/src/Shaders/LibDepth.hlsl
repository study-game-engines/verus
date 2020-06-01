// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

// Convert non-linear z-buffer value to positive linear form:
float ToLinearDepth(float d, float4 zNearFarEx)
{
	// INFO: zNearFarEx.z = zFar/(zFar-zNear)
	// INFO: zNearFarEx.w = zFar*zNear/(zNear-zFar)
	return zNearFarEx.w / (d - zNearFarEx.z);
}

float4 ToLinearDepth(float4 d, float4 zNearFarEx)
{
	return zNearFarEx.w / (d - zNearFarEx.z);
}

float ComputeFog(float depth, float density, float height = 0.0)
{
	const float strength = 1.0 - saturate(height * 0.003);
	const float power = depth * lerp(0.0, density, strength * strength);
	const float fog = 1.0 / exp(power * power);
	return 1.0 - saturate(fog);
}

float3 AdjustPosForShadow(float3 pos, float3 normal, float3 dirToLight, float depth)
{
	const float scale = depth - 5.0;
	return pos + normal * 0.012 * max(1.0, scale * 0.2) + dirToLight * max(0.0, scale * 0.002);
}

float4 ShadowCoords(float4 pos, matrix mat, float depth)
{
	// For CSM transformation is done in PS.
#if _SHADOW_QUALITY >= 4
	return float4(pos.xyz, depth);
#else
	return mul(pos, mat);
#endif
}

bool IsValidShadowSample(int x, int y)
{
	int sum = abs(x) + abs(y);
	if (4 == sum)
		return false;
#if _SHADOW_QUALITY <= 4
	if (1 == sum % 2)
		return false;
#endif
	return true;
}

float PCF(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float3 tc,
	float4 config)
{
#if _SHADOW_QUALITY <= 2
	return texCmp.SampleCmpLevelZero(samCmp, tc.xy, tc.z).r;
#else
	const float dzFrag = tc.z;
	float2 dzBlockers_blockerCount = 0.0;
	float sum = 0.0;
	int count = 0;
	[unroll] for (int y = -2; y <= 2; ++y)
	{
		[unroll] for (int x = -2; x <= 2; ++x)
		{
			if (!IsValidShadowSample(x, y))
				continue;
			count++;

			const float alpha = texCmp.SampleCmpLevelZero(samCmp, tc.xy, dzFrag, int2(x, y)).r;
			sum += alpha;

			const float dzBlocker = tex.SampleLevel(sam, tc.xy, 0, int2(x, y)).r;
			if (dzBlocker < dzFrag)
				dzBlockers_blockerCount += float2(dzBlocker, 1);
		}
	}
	const float ret = sum * (1.0 / count);
	if (dzBlockers_blockerCount.y > 0.0)
	{
		const float penumbraScale = config.x;
		const float penumbraContrastMask = config.y;
		const float dzBlockers = dzBlockers_blockerCount.x / dzBlockers_blockerCount.y;
		const float penumbra = saturate(dzFrag - dzBlockers);
		const float contrast = 1.0 + max(0.0, 3.0 - penumbra * penumbraScale) * penumbraContrastMask;
		return saturate((ret - 0.5) * contrast + 0.5);
	}
	else
	{
		return ret;
	}
#endif
}

float SimplePCF(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float3 tc)
{
	return texCmp.SampleCmpLevelZero(samCmp, tc.xy, tc.z).r;
}

float ShadowMap(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float4 tc,
	float4 config)
{
#if _SHADOW_QUALITY <= 0
	return 1.0;
#else
	const float ret = PCF(texCmp, samCmp, tex, sam, tc.xyz, config);
	const float2 center = tc.xy * 2.0 - 1.0;
	return max(ret, saturate(dot(center, center) * (9.0 / 4.0) - 1.0));
#endif
}

float ShadowMapCSM(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float4 pos,
	float4 config,
	float4 ranges,
	matrix mat0,
	matrix mat1,
	matrix mat2,
	matrix mat3)
{
#if _SHADOW_QUALITY >= 4
	float ret = 1.0;
	const float4 p = float4(pos.xyz, 1);
	float contrast = 1.0;
	const float contrastScale = config.w;

	if (pos.w > ranges.x)
	{
		const float fadeStart = (ranges.x + ranges.w) * 0.5;
		const float fade = saturate((pos.w - fadeStart) / (ranges.w - fadeStart));

		contrast = contrastScale * contrastScale * contrastScale;
		const float3 tc = mul(p, mat0).xyz;
		ret = max(PCF(texCmp, samCmp, tex, sam, tc, config), fade);
	}
	else if (pos.w > ranges.y)
	{
		contrast = contrastScale * contrastScale;
		const float3 tc = mul(p, mat1).xyz;
		ret = PCF(texCmp, samCmp, tex, sam, tc, config);
	}
	else if (pos.w > ranges.z)
	{
		contrast = contrastScale;
		const float3 tc = mul(p, mat2).xyz;
		ret = PCF(texCmp, samCmp, tex, sam, tc, config);
	}
	else
	{
		const float3 tc = mul(p, mat3).xyz;
		ret = PCF(texCmp, samCmp, tex, sam, tc, config);
	}

	return saturate((ret - 0.5) * contrast + 0.5);
#else
	return ShadowMap(texCmp, samCmp, tex, sam, pos, config);
#endif
}

float SimpleShadowMapCSM(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float4 pos,
	float4 config,
	float4 ranges,
	matrix mat0,
	matrix mat1,
	matrix mat2,
	matrix mat3)
{
#if _SHADOW_QUALITY >= 4
	float ret = 1.0;
	const float4 p = float4(pos.xyz, 1);
	float contrast = 1.0;
	const float contrastScale = config.w;

	if (pos.w > ranges.x)
	{
		const float fadeStart = (ranges.x + ranges.w) * 0.5;
		const float fade = saturate((pos.w - fadeStart) / (ranges.w - fadeStart));

		contrast = contrastScale * contrastScale * contrastScale;
		const float3 tc = mul(p, mat0).xyz;
		ret = max(SimplePCF(texCmp, samCmp, tc), fade);
	}
	else if (pos.w > ranges.y)
	{
		contrast = contrastScale * contrastScale;
		const float3 tc = mul(p, mat1).xyz;
		ret = SimplePCF(texCmp, samCmp, tc);
	}
	else if (pos.w > ranges.z)
	{
		contrast = contrastScale;
		const float3 tc = mul(p, mat2).xyz;
		ret = SimplePCF(texCmp, samCmp, tc);
	}
	else
	{
		const float3 tc = mul(p, mat3).xyz;
		ret = SimplePCF(texCmp, samCmp, tc);
	}

	return saturate((ret - 0.5) * contrast + 0.5);
#else
	return SimplePCF(texCmp, samCmp, pos.xyz);
#endif
}
