// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		enum class NodeType : int
		{
			unknown,

			base,

			model,
			particles,

			block,
			controlPoint,
			emitter,
			instance,
			light,
			path,
			physics,
			prefab,
			shaker,
			sound,
			terrain,

			trigger,

			site,

			character,
			vehicle,

			count
		};
	}
}
