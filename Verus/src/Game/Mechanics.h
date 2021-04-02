// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class Mechanics
		{
		public:
			Mechanics();
			virtual ~Mechanics();

			virtual void OnBegin() {}
			virtual void OnEnd() {}
			virtual bool CanAutoPop() { return false; }

			virtual Continue HandleInput() { return Continue::yes; }
			virtual Continue Update() { return Continue::yes; }
			virtual Continue Draw() { return Continue::yes; }
			virtual Continue DrawOverlay() { return Continue::yes; }

			virtual void ApplyReport(const void* pReport) {}
			virtual Continue GetBotDomainCenter(int id, RPoint3 center) { return Continue::yes; }
			virtual Continue GetSpawnPosition(int id, RPoint3 pos) { return Continue::yes; };
			virtual bool IsInputEnabled() { return true; }
			virtual Continue OnDie(int id) { return Continue::yes; }
			virtual Continue OnMouseMove(float x, float y) { return Continue::yes; }
			virtual Continue OnTakeDamage(int id, float amount) { return Continue::yes; }
			virtual Continue UpdateMultiplayer() { return Continue::yes; }
			virtual Scene::PMainCamera GetMainCamera() { return nullptr; }
		};
		VERUS_TYPEDEFS(Mechanics);
	}
}
