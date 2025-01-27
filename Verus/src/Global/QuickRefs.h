// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_QREF_ASYNC          IO::RAsync              async    = IO::Async::I()
#define VERUS_QREF_ASYS           Audio::RAudioSystem     asys     = Audio::AudioSystem::I()
#define VERUS_QREF_ATMO           World::RAtmosphere      atmo     = World::Atmosphere::I()
#define VERUS_QREF_BLOOM          Effects::RBloom         bloom    = Effects::Bloom::I()
#define VERUS_QREF_BLUR           Effects::RBlur          blur     = Effects::Blur::I()
#define VERUS_QREF_BULLET         Physics::RBullet        bullet   = Physics::Bullet::I()
#define VERUS_QREF_CINEMA         Effects::RCinema        cinema   = Effects::Cinema::I()
#define VERUS_QREF_CONST_SETTINGS App::RcSettings         settings = App::Settings::IConst()
#define VERUS_QREF_DD             CGI::RDebugDraw         dd       = CGI::DebugDraw::I()
#define VERUS_QREF_EO             World::REditorOverlays  eo       = World::EditorOverlays::I()
#define VERUS_QREF_FSYS           IO::RFileSystem         fsys     = IO::FileSystem::I()
#define VERUS_QREF_GRASS          World::RGrass           grass    = World::Grass::I()
#define VERUS_QREF_IM             Input::RInputManager    im       = Input::InputManager::I()
#define VERUS_QREF_LMB            World::RLightMapBaker   lmb      = World::LightMapBaker::I()
#define VERUS_QREF_MM             World::RMaterialManager mm       = World::MaterialManager::I()
#define VERUS_QREF_MP             Net::RMultiplayer       mp       = Net::Multiplayer::I()
#define VERUS_QREF_RENDERER       CGI::RRenderer          renderer = CGI::Renderer::I()
#define VERUS_QREF_SETTINGS       App::RSettings          settings = App::Settings::I()
#define VERUS_QREF_SSAO           Effects::RSsao          ssao     = Effects::Ssao::I()
#define VERUS_QREF_SSR            Effects::RSsr           ssr      = Effects::Ssr::I()
#define VERUS_QREF_TIMER          RTimer                  timer    = Timer::I(); const float dt = timer.GetDeltaTime()
#define VERUS_QREF_TIMER_GUI      RTimer                  timer    = Timer::I(); const float dt = timer.GetDeltaTime(Timer::Type::gui)
#define VERUS_QREF_UTILS          RUtils                  utils    = Utils::I()
#define VERUS_QREF_VM             GUI::RViewManager       vm       = GUI::ViewManager::I()
#define VERUS_QREF_WATER          World::RWater           water    = World::Water::I()
#define VERUS_QREF_WM             World::RWorldManager    wm       = World::WorldManager::I()
#define VERUS_QREF_WU             World::RWorldUtils      wu       = World::WorldUtils::I()
