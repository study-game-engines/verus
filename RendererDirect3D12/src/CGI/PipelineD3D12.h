#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineD3D12 : public BasePipeline
		{
			ComPtr<ID3D12PipelineState> _pPipelineState;

		public:
			PipelineD3D12();
			virtual ~PipelineD3D12() override;

			virtual void Init() override;
			virtual void Done() override;
		};
	}
}