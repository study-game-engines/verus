#pragma once

namespace verus
{
	namespace CGI
	{
		enum class Gapi : int
		{
			unknown,
			vulkan,
			direct3D12
		};

		struct BaseRendererDesc
		{
			GlobalVarsClipboard _gvc;

			BaseRendererDesc()
			{
				_gvc.Copy();
			}
		};
		VERUS_TYPEDEFS(BaseRendererDesc);

		class BaseRenderer : public Object
		{
		protected:
			Vector<PScheduled> _vScheduled;
			BaseRendererDesc   _desc;
			UINT32             _swapChainBufferCount = 0;
			UINT32             _swapChainBufferIndex = 0;
			UINT32             _ringBufferIndex = 0;

			BaseRenderer();
			virtual ~BaseRenderer();

		public:
			static const int s_ringBufferSize = 3;

			static BaseRenderer* Load(CSZ dll, RBaseRendererDesc desc);
			virtual void ReleaseMe() = 0;

			void SetDesc(RBaseRendererDesc desc) { _desc = desc; }

			UINT32 GetSwapChainBufferCount() const { return _swapChainBufferCount; }
			UINT32 GetSwapChainBufferIndex() const { return _swapChainBufferIndex; }
			UINT32 GetRingBufferIndex() const { return _ringBufferIndex; }

			void Schedule(PScheduled p);
			void Unschedule(PScheduled p);
			void UpdateScheduled();

			virtual void ImGuiInit(int renderPassHandle) = 0;
			virtual void ImGuiRenderDrawData() = 0;

			virtual void ResizeSwapChain() = 0;

			// Which graphics API?
			virtual Gapi GetGapi() = 0;

			// Frame cycle:
			virtual void BeginFrame(bool present = true) = 0;
			virtual void EndFrame(bool present = true) = 0;
			virtual void Present() = 0;
			virtual void WaitIdle() = 0;

			// Resources:
			virtual PBaseCommandBuffer InsertCommandBuffer() = 0;
			virtual PBaseGeometry      InsertGeometry() = 0;
			virtual PBasePipeline      InsertPipeline() = 0;
			virtual PBaseShader        InsertShader() = 0;
			virtual PBaseTexture       InsertTexture() = 0;

			virtual void DeleteCommandBuffer(PBaseCommandBuffer p) = 0;
			virtual void DeleteGeometry(PBaseGeometry p) = 0;
			virtual void DeletePipeline(PBasePipeline p) = 0;
			virtual void DeleteShader(PBaseShader p) = 0;
			virtual void DeleteTexture(PBaseTexture p) = 0;

			virtual int CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD) = 0;
			virtual int CreateFramebuffer(int renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex = -1) = 0;
			virtual void DeleteRenderPass(int handle) = 0;
			virtual void DeleteFramebuffer(int handle) = 0;

			static void SetAlphaBlendHelper(
				CSZ sz,
				int& colorBlendOp,
				int& alphaBlendOp,
				int& srcColorBlendFactor,
				int& dstColorBlendFactor,
				int& srcAlphaBlendFactor,
				int& dstAlphaBlendFactor);
		};
		VERUS_TYPEDEFS(BaseRenderer);
	}

	extern "C"
	{
		typedef CGI::PBaseRenderer(*PFNCREATERENDERER)(UINT32 version, CGI::BaseRendererDesc* pDesc);
	}
}
