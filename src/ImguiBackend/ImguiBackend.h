#pragma once

#include <RHI/RHI.h>

struct ImguiProcessEventResult
{
	bool wantsCaptureKeyboard = false;
	bool wantsCaptureMouse = false;
};

struct ImguiRenderResult
{
	class RHICommandList* cl = nullptr;
	uint64_t frame_idx = 0;
};

class ImguiBackend
{
private:
	class RHI* m_rhi = nullptr;

	RHITexturePtr m_fonts_atlas = nullptr;
	RHITextureSRVPtr m_fonts_atlas_srv = nullptr;
	RHISamplerPtr m_fonts_sampler = nullptr;

	RHIGraphicsPipelinePtr m_pso = nullptr;

	uint64_t m_cur_frame = 1;

	uint64_t m_submitted_frame = 0;

	struct FrameData
	{
		RHIUploadBufferPtr vertices = nullptr;
		RHIUploadBufferPtr indices = nullptr;
		uint64_t submitted_frame_idx = ~0x0;
	};

	RHIShaderBindingTablePtr m_sbt = nullptr;

	std::vector<FrameData> m_cache;

public:
	ImguiBackend( struct SDL_Window* window, RHI* rhi, RHIFormat target_format );
	~ImguiBackend();

	ImguiProcessEventResult ProcessEvent( union SDL_Event* event );

	ImguiRenderResult RenderFrame( class RHIRTV& rtv );

	// We try to reuse buffer allocations, so we have to know when a frame is done rendering.
	void MarkFrameAsCompleted( uint64_t frame_idx );

private:
	void NewFrame();
	void SetupFonts();

	void SetupPSO( RHIFormat target_format );

	void RecordCommandList(
		struct ImDrawData* draw_data, 
		uint32_t fb_width, uint32_t fb_height,
		RHIBuffer* vtx_buf, RHIBuffer* idx_buf,
		RHICommandList& cl, RHIRTV& rtv ) const;

	// Can allocate new cache entry
	FrameData* FindFittingCache( size_t vertex_buf_size, size_t index_buf_size );
};