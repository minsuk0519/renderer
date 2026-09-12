#pragma once

#include <system/defines.hpp>
#include <vector>
#include <render/render_memlayout.hpp>

#if ENGINE_DEBUG_RESOURCEVIEW
#include <d3d12.h>
#include <wrl.h>
#endif // ENGINE_DEBUG_RESOURCEVIEW

struct buffer;

namespace render
{
#if ENGINE_DEBUG_RESOURCEVIEW

	enum TEXPREVIEW_MODE : uint
	{
		TEXPREVIEW_RAW = 0,
		TEXPREVIEW_OCT_NORMAL,
		TEXPREVIEW_OBJECT_ID,
		TEXPREVIEW_VIS_ID,
		TEXPREVIEW_RED_GRAY,
		TEXPREVIEW_MODE_COUNT,
	};

	struct texPreviewInfo
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
		uint  mip = 0, width = 0, height = 0;
		float u1 = 1.0f, v1 = 1.0f;
	};

	void setSelectedResourceId(uint bufferId);
	uint getSelectedResourceId();
	void updateTexturePreview();
	bool isTexturePreviewable(uint bufferId, const char** outReason);
	bool getTexturePreviewInfo(texPreviewInfo& out);
	void setTexturePreviewMip(int mip);
	int  getTexturePreviewMip();
	void setTexturePreviewMode(TEXPREVIEW_MODE mode);
	TEXPREVIEW_MODE getTexturePreviewMode();
	void requestTexturePreviewCopy();
#endif // ENGINE_DEBUG_RESOURCEVIEW

#if ENGINE_DEBUG_READBACK
	constexpr uint DEBUG_READBACK_STATS_BYTES = sizeof(uint) * 32;
	constexpr uint DEBUG_READBACK_MEMVIEW_BYTES = 64u * 1024u * 1024u;
#if ENGINE_DEBUG_MEMVIEW
	constexpr uint DEBUG_READBACK_BUFFER_SIZE = DEBUG_READBACK_MEMVIEW_BYTES;
#else
	constexpr uint DEBUG_READBACK_BUFFER_SIZE = DEBUG_READBACK_STATS_BYTES;
#endif // ENGINE_DEBUG_MEMVIEW
#endif // ENGINE_DEBUG_READBACK
}  // namespace render

#if ENGINE_DEBUG_READBACK
class renderDebug
{
public:
	void update();
	buffer* getDebugReadBackBuffer();

#if ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW
	void guiMemoryReadbackSetting();
#endif // ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW

private:
	void ensureDebugReadBackBuffer();
	buffer* debugReadBackBuffer = nullptr;
	bool debugReadBackBufferAttempted = false;

#if ENGINE_DEBUG_MEMVIEW
	void requestMemReadback(buffer* target, uint byteCount);
	const std::vector<unsigned char>& getMemReadbackData() const;
	uint getMemReadbackResultId() const;
	bool getMemReadbackFailed() const;

	bool memReadbackRequest = false;
	buffer* memReadbackTarget = nullptr;
	uint memReadbackByteCount = 0;
	std::vector<unsigned char> memReadbackData;
	uint memReadbackResultId = ~0u;
	bool memReadbackFailed = false;

	void ensureMemLayouts();
	std::vector<render::memLayout> memLayouts;
	bool memLayoutsAttempted = false;
	int selectedMemLayoutIndex = -1;
	uint lastReadbackSelectionId = ~0u;

#if ENGINE_DEBUG_RESOURCEVIEW
	void guiUpdateButton(uint selectedId, bool selIsBuffer, bool selIsTexture);
#endif // ENGINE_DEBUG_RESOURCEVIEW

#endif // ENGINE_DEBUG_MEMVIEW
};

#endif // ENGINE_DEBUG_READBACK
