#pragma once

namespace verus
{
	namespace Scene
	{
		class Forest : public Object, public ScatterDelegate, public Math::OctreeDelegate
		{
#include "../Shaders/DS_Forest.inc.hlsl"

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_DEPTH,
				PIPE_COUNT
			};

		public:
			enum SCATTER_TYPE
			{
				SCATTER_TYPE_100,
				SCATTER_TYPE_50,
				SCATTER_TYPE_25,
				SCATTER_TYPE_20,
				SCATTER_TYPE_15,
				SCATTER_TYPE_10,
				SCATTER_TYPE_5,
				SCATTER_TYPE_3,
				SCATTER_TYPE_COUNT
			};

		private:
			struct Vertex
			{
				glm::vec3 _pos;
				short     _tc[2]; // psize, angle.
			};
			VERUS_TYPEDEFS(Vertex);

			class BakedChunk
			{
			public:
				Vector<Vertex> _vSprites;
				int            _vbOffset = 0;
				bool           _visible = false;
			};
			VERUS_TYPEDEFS(BakedChunk);

			class Plant
			{
			public:
				enum TEX
				{
					TEX_GBUFFER_0,
					TEX_GBUFFER_1,
					TEX_GBUFFER_2,
					TEX_COUNT
				};

				String                      _url;
				Mesh                        _mesh;
				MaterialPwn                 _material;
				CGI::TexturePwns<TEX_COUNT> _tex;
				CGI::CSHandle               _csh;
				Vector<BakedChunk>          _vBakedChunks;
				Vector<float>               _vScales;
				float                       _alignToNormal = 1;
				float                       _maxScale = 0;
				float                       _maxSize = 0;
				float                       _aoSize = 1;
				char                        _allowedNormal = 116;

				float GetSize() const;
			};
			VERUS_TYPEDEFS(Plant);

			class LayerPlants
			{
			public:
				char _plants[SCATTER_TYPE_COUNT];

				LayerPlants()
				{
					std::fill(_plants, _plants + SCATTER_TYPE_COUNT, -1);
				}
			};
			VERUS_TYPEDEFS(LayerPlants);

			class DrawPlant
			{
			public:
				Matrix3 _basis = Matrix3::identity();
				Point3  _pos = Point3(0);
				Vector3 _pushBack = Vector3(0);
				float   _scale = 1;
				float   _angle = 0;
				float   _distToEyeSq = 0;
				int     _plantIndex = 0;
			};
			VERUS_TYPEDEFS(DrawPlant);

			static CGI::ShaderPwn s_shader;
			static UB_ForestVS    s_ubForestVS;
			static UB_ForestFS    s_ubForestFS;

			PTerrain                      _pTerrain = nullptr;
			CGI::GeometryPwn              _geo;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			Math::Octree                  _octree;
			Scatter                       _scatter;
			Vector<Plant>                 _vPlants;
			Vector<LayerPlants>           _vLayerPlants;
			Vector<DrawPlant>             _vDrawPlants;
			const float                   _margin = 1.1f;
			float                         _maxDist = 100;
			float                         _tessDist = 50;
			float                         _maxSizeAll = 0;
			int                           _capacity = 4000;
			int                           _visibleCount = 0;
			bool                          _async_initPlants = false;

		public:
			class PlantDesc
			{
			public:
				CSZ   _url = nullptr;
				float _alignToNormal = 1;
				float _minScale = 0.5f;
				float _maxScale = 1.5f;
				char  _allowedNormal = 116;

				void Set(CSZ url, float alignToNormal = 1, float minScale = 0.5f, float maxScale = 1.5f, char allowedNormal = 116)
				{
					_url = url;
					_alignToNormal = alignToNormal;
					_minScale = minScale;
					_maxScale = maxScale;
					_allowedNormal = allowedNormal;
				}
			};
			VERUS_TYPEDEFS(PlantDesc);

			class LayerDesc
			{
			public:
				PlantDesc _forType[SCATTER_TYPE_COUNT];

				void Reset() { *this = LayerDesc(); }
			};
			VERUS_TYPEDEFS(LayerDesc);

			Forest();
			~Forest();

			static void InitStatic();
			static void DoneStatic();

			void Init(PTerrain pTerrain);
			void Done();

			void ResetInstanceCount();
			void Update();
			void Layout();
			void Draw(bool allowTess = true);
			VERUS_P(void DrawModels(bool allowTess));
			VERUS_P(void DrawSprites());
			void DrawAO();

			PTerrain SetTerrain(PTerrain p) { return Utils::Swap(_pTerrain, p); }

			void SetLayer(int layer, RcLayerDesc desc);
			VERUS_P(int FindPlant(CSZ url) const);
			VERUS_P(void BakeChunks(RPlant plant));
			bool LoadSprite(RPlant plant);
			void BakeSprite(RPlant plant, CSZ url);
			bool BakeSprites(CSZ url);

			virtual void Scatter_AddInstance(const int ij[2], int type, float x, float z,
				float scale, float angle, UINT32 r) override;

			virtual Continue Octree_ProcessNode(void* pToken, void* pUser) override;
		};
		VERUS_TYPEDEFS(Forest);
	}
}
