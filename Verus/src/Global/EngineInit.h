// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Input
	{
		struct KeyMapperDelegate;
	}
	namespace CGI
	{
		struct RendererDelegate;
	}
}

namespace verus
{
	class EngineInit
	{
	public:
		bool _makeAudio = true;
		bool _makeCGI = true;
		bool _makeEffects = true;
		bool _makeExtra = false;
		bool _makeGlobal = true;
		bool _makeGUI = true;
		bool _makeInput = true;
		bool _makeIO = true;
		bool _makeNet = true;
		bool _makePhysics = true;
		bool _makeScene = true;
		bool _allowInitShaders = true;

		void Make();
		void Free();

		void Init(Input::KeyMapperDelegate* pKeyMapperDelegate, CGI::RendererDelegate* pRendererDelegate);
		void InitCmd();

		void ReducedFeatureSet();
	};
	VERUS_TYPEDEFS(EngineInit);
}
