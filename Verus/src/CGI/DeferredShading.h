#pragma once

namespace verus
{
	namespace CGI
	{
		enum class LightType : int
		{
			none,
			dir,
			omni,
			spot
		};

		class DeferredShading : public Object
		{
#include "../Shaders/DS.inc.hlsl"
#include "../Shaders/DS_Compose.inc.hlsl"

			enum SHADER
			{
				SHADER_LIGHT,
				SHADER_COMPOSE,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_INSTANCED_DIR,
				PIPE_INSTANCED_OMNI,
				PIPE_INSTANCED_SPOT,
				PIPE_COMPOSE,
				PIPE_TONE_MAPPING,
				PIPE_QUAD,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_GBUFFER_0, // Albedo, Spec.
				TEX_GBUFFER_1, // RG - Normals, Emission, Motion.
				TEX_GBUFFER_2, // LamScale, LamBias, Metallicity, Gloss.
				TEX_LIGHT_ACC_DIFF,
				TEX_LIGHT_ACC_SPEC,
				TEX_COMPOSED_A,
				TEX_COMPOSED_B,
				TEX_COUNT
			};

			static UB_PerFrame   s_ubPerFrame;
			static UB_TexturesFS s_ubTexturesFS;
			static UB_PerMeshVS  s_ubPerMeshVS;
			static UB_ShadowFS   s_ubShadowFS;
			static UB_PerObject  s_ubPerObject;
			static UB_ComposeVS  s_ubComposeVS;
			static UB_ComposeFS  s_ubComposeFS;

			ShaderPwns<SHADER_COUNT> _shader;
			PipelinePwns<PIPE_COUNT> _pipe;
			TexturePwns<TEX_COUNT>   _tex;
			TexturePtr               _texShadowAtmo;
			UINT64                   _frame = 0;
			RPHandle                 _rph;
			RPHandle                 _rphCompose;
			RPHandle                 _rphExtraCompose;
			FBHandle                 _fbh;
			FBHandle                 _fbhCompose;
			FBHandle                 _fbhExtraCompose;
			CSHandle                 _cshLight;
			CSHandle                 _cshCompose;
			CSHandle                 _cshToneMapping;
			CSHandle                 _cshQuad[5];
			bool                     _activeGeometryPass = false;
			bool                     _activeLightingPass = false;
			bool                     _async_initPipe = false;

		public:
			DeferredShading();
			~DeferredShading();

			void Init();
			void InitGBuffers(int w, int h);
			void InitByAtmosphere(TexturePtr texShadow);
			void Done();

			void OnSwapChainResized(bool init, bool done);

			static bool IsLoaded();

			void Draw(int gbuffer);

			bool IsActiveGeometryPass() const { return _activeGeometryPass; }
			bool IsActiveLightingPass() const { return _activeLightingPass; }

			RPHandle GetRenderPassHandle() const { return _rph; }
			RPHandle GetRenderPassHandle_ExtraCompose() const { return _rphExtraCompose; }

			void BeginGeometryPass(bool onlySetRT = false, bool spriteBaking = false);
			void EndGeometryPass(bool resetRT = false);
			bool BeginLightingPass();
			void EndLightingPass();
			void BeginCompose();
			void EndCompose();
			void ToneMapping(RcVector4 bgColor = Vector4(0));
			void AntiAliasing();

			static bool IsLightUrl(CSZ url);
			static UB_PerMeshVS& GetUbPerMeshVS() { return s_ubPerMeshVS; }
			void OnNewLightType(LightType type, bool wireframe = false);
			void UpdateUniformBufferPerFrame();
			void BindDescriptorsPerMeshVS();

			void Load();

			TexturePtr GetGBuffer(int index);

			TexturePtr GetComposedTextureA();
			TexturePtr GetComposedTextureB();
		};
		VERUS_TYPEDEFS(DeferredShading);
	}
}
