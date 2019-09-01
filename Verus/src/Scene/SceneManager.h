#pragma once

namespace verus
{
	namespace Scene
	{
		class SceneManager : public Singleton<SceneManager>, public Object
		{
			Math::Octree _octree;
			PCamera      _pCamera = nullptr;
			PMainCamera  _pMainCamera = nullptr;

		public:
			SceneManager();
			virtual ~SceneManager();

			void Init();
			void Done();

			// Camera:
			PCamera GetCamera() const { return _pCamera; }
			PCamera SetCamera(PCamera p) { return Utils::Swap(_pCamera, p); }
			PMainCamera GetMainCamera() const { return _pMainCamera; }
			PMainCamera SetCamera(PMainCamera p) { _pCamera = p; return Utils::Swap(_pMainCamera, p); }
		};
		VERUS_TYPEDEFS(SceneManager);
	}
}
