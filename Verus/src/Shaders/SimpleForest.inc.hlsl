// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_SimpleForestVS
{
	matrix _matP;
	matrix _matWVP;
	float4 _viewportSize;
	float4 _eyePos;
	float4 _eyePosScreen;
	float4 _pointSpriteScale;
};

VERUS_UBUFFER UB_SimpleForestFS
{
	mataff _matInvV;
	float4 _ambientColor;
	float4 _fogColor;
	float4 _dirToSun;
	float4 _sunColor;
	matrix _matSunShadow;
	matrix _matSunShadowCSM1;
	matrix _matSunShadowCSM2;
	matrix _matSunShadowCSM3;
	float4 _shadowConfig;
	float4 _splitRanges;
};