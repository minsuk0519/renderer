#include <render/render_debug.hpp>

#if ENGINE_DEBUG_RESOURCEVIEW

#include <render/buffer.hpp>
#include <render/descriptorheap.hpp>
#include <render/renderer.hpp>
#include <system/logger.hpp>
#include <algorithm>
#include <render/commandqueue.hpp>
#include <render/shader_defines.hpp>
#include <system/eventMarker.hpp>

namespace render
{
	static uint selectedResourceId = ~0u;

	void setSelectedResourceId(uint bufferId)
	{
		selectedResourceId = bufferId;
	}

	uint getSelectedResourceId()
	{
		return selectedResourceId;
	}

	// Texture preview state — compute shader model
	constexpr uint TEXPREVIEW_MAX_DIM = 2048;

	static buffer* texPreviewScratch = nullptr;
	static bool    texPreviewScratchAttempted = false;

	static bool texPreviewCopyRequest = false;
	static bool texPreviewHasSnapshot = false;
	static uint texPreviewSnapshotId = ~0u;
	static int  texPreviewSnapshotMip = -1;
	static uint texPreviewSnapshotWidth = 0;   // used sub-rect of the scratch
	static uint texPreviewSnapshotHeight = 0;
	static int  texPreviewMip = 0;
	static TEXPREVIEW_MODE texPreviewMode = TEXPREVIEW_RAW;
	static TEXPREVIEW_MODE texPreviewSnapshotMode = TEXPREVIEW_RAW;

	static bool isIntegerFormat(DXGI_FORMAT fmt)
	{
		switch (fmt)
		{
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R8G8B8A8_SINT:
			return true;
		default:
			return false;
		}
	}

	static void ensureTexturePreviewScratch()
	{
		if (texPreviewScratchAttempted)
		{
			return;
		}
		texPreviewScratchAttempted = true;

		texPreviewScratch = e_globBufAllocator.alloc(nullptr, 0, 1,
			buf::GBF_SRV | buf::GBF_UAV, buf::RESOURCE_TEXTURE,
			DXGI_FORMAT_R8G8B8A8_UNORM, TEXPREVIEW_MAX_DIM, TEXPREVIEW_MAX_DIM, 1,
			DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f), nullptr, 0, 0, "texture preview scratch");

		if (texPreviewScratch == nullptr)
		{
			TC_LOG_ERROR("Texture preview: failed to allocate scratch buffer");
		}
	}

	static buffer* resolveTexPreviewSrvOwner(uint bufferId)
	{
		buffer* owner = buf::getResourceOwner(bufferId);
		if (owner == nullptr)
		{
			return nullptr;
		}
		if (owner->getDesc(buf::GBF_SRV) != nullptr)
		{
			return owner;
		}

		// No SRV on this wrapper - look for a sibling wrapper over the same resource that has one.
		ID3D12Resource* target = owner->getResource();
		for (uint id = 0; id < buf::getResourceDebugInfoCount(); ++id)
		{
			if (id == bufferId)
			{
				continue;
			}
			buffer* other = buf::getResourceOwner(id);
			if (other != nullptr && other->getResource() == target && other->getDesc(buf::GBF_SRV) != nullptr)
			{
				return other;
			}
		}
		return nullptr;
	}

	bool isTexturePreviewable(uint bufferId, const char** outReason)
	{
		if (bufferId >= buf::getResourceDebugInfoCount() || buf::getResourceOwner(bufferId) == nullptr)
		{
			if (outReason != nullptr)
			{
				*outReason = "no live resource";
			}
			return false;
		}

		if (!buf::isTextureResource(bufferId))
		{
			if (outReason != nullptr)
			{
				*outReason = "not a texture";
			}
			return false;
		}

		buffer* owner = buf::getResourceOwner(bufferId);
		ID3D12Resource* res = owner->getResource();
		D3D12_RESOURCE_DESC desc = res->GetDesc();

		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			if (outReason != nullptr)
			{
				*outReason = "only TEXTURE2D is previewable";
			}
			return false;
		}

		if (desc.Format == DXGI_FORMAT_D32_FLOAT ||
		    desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
		    desc.Format == DXGI_FORMAT_D16_UNORM ||
		    desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
		{
			if (outReason != nullptr)
			{
				*outReason = "depth formats are not previewable";
			}
			return false;
		}

		if (resolveTexPreviewSrvOwner(bufferId) == nullptr)
		{
			if (outReason != nullptr)
			{
				*outReason = "no SRV on this resource or any alias of it";
			}
			return false;
		}

		return true;
	}

	void updateTexturePreview()
	{
		uint id = render::getSelectedResourceId();

		// Discard stale snapshot on selection change
		if (texPreviewHasSnapshot && id != texPreviewSnapshotId)
		{
			texPreviewHasSnapshot = false;
		}

		if (!texPreviewCopyRequest)
		{
			return;
		}

		texPreviewCopyRequest = false;

		ensureTexturePreviewScratch();
		if (texPreviewScratch == nullptr)
		{
			return;
		}
		if (!isTexturePreviewable(id, nullptr))
		{
			return;
		}

		buffer* owner = buf::getResourceOwner(id);
		buffer* srvOwner = resolveTexPreviewSrvOwner(id);
		if (srvOwner == nullptr)
		{
			return;
		}

		ID3D12Resource* res = owner->getResource();
		D3D12_RESOURCE_DESC desc = res->GetDesc();

		texPreviewMip = (std::max)(0, (std::min)(texPreviewMip, (int)desc.MipLevels - 1));

		uint srcW = (std::max)(1u, (uint)(desc.Width >> texPreviewMip));
		uint srcH = (std::max)(1u, (uint)(desc.Height >> texPreviewMip));

		// Fit into the scratch preserving aspect
		float scale = (std::min)(1.0f, (std::min)((float)TEXPREVIEW_MAX_DIM / (float)srcW,
		                                           (float)TEXPREVIEW_MAX_DIM / (float)srcH));
		uint dstW = (std::max)(1u, (uint)((float)srcW * scale));
		uint dstH = (std::max)(1u, (uint)((float)srcH * scale));

		// Dispatch the compute shader
		auto computeCmdList = render::getCmdQueue(render::QUEUE_COMPUTE)->getCmdList();
		render::getCmdQueue(render::QUEUE_COMPUTE)->bindPSO(
			isIntegerFormat(desc.Format) ? render::PSO_TEXPREVIEWUINT : render::PSO_TEXPREVIEWFLOAT);
		{
			GPU_EVENT(computeCmdList.Get(), "TexturePreview");

			if (texPreviewScratch->getCurResourceState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			{
				CD3DX12_RESOURCE_BARRIER b = texPreviewScratch->getTransition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				computeCmdList->ResourceBarrier(1, &b);
			}

			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(UAV_TEXPREVIEW, texPreviewScratch, buf::GBF_UAV);
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(SRV_TEXPREVIEW_SRC, srvOwner, buf::GBF_SRV);

			uint cbData[6] = { (uint)texPreviewMode, (uint)texPreviewMip, srcW, srcH, dstW, dstH };
			render::getCmdQueue(render::QUEUE_COMPUTE)->sendData(CBV_TEXPREVIEW, 6, cbData);

			computeCmdList->Dispatch((dstW + 7) / 8, (dstH + 7) / 8, 1);

			CD3DX12_RESOURCE_BARRIER b2 = texPreviewScratch->getTransition(D3D12_RESOURCE_STATE_COMMON);
			computeCmdList->ResourceBarrier(1, &b2);
		}
		render::getCmdQueue(render::QUEUE_COMPUTE)->execute({ computeCmdList });
		render::getCmdQueue(render::QUEUE_COMPUTE)->flush();

		texPreviewHasSnapshot = true;
		texPreviewSnapshotId = id;
		texPreviewSnapshotMip = texPreviewMip;
		texPreviewSnapshotMode = texPreviewMode;
		texPreviewSnapshotWidth = dstW;
		texPreviewSnapshotHeight = dstH;
	}

	bool getTexturePreviewInfo(texPreviewInfo& out)
	{
		if (!texPreviewHasSnapshot || texPreviewScratch == nullptr)
		{
			return false;
		}

		out.handle = texPreviewScratch->getDesc(buf::GBF_SRV)->getHandle();
		out.mip    = (uint)texPreviewSnapshotMip;
		out.width  = texPreviewSnapshotWidth;
		out.height = texPreviewSnapshotHeight;
		out.u1     = (float)texPreviewSnapshotWidth  / (float)TEXPREVIEW_MAX_DIM;
		out.v1     = (float)texPreviewSnapshotHeight / (float)TEXPREVIEW_MAX_DIM;
		return true;
	}

	void requestTexturePreviewCopy()
	{
		texPreviewCopyRequest = true;
	}

	void setTexturePreviewMip(int mip)
	{
		texPreviewMip = mip;
	}

	int getTexturePreviewMip()
	{
		return texPreviewMip;
	}

	void setTexturePreviewMode(TEXPREVIEW_MODE mode)
	{
		texPreviewMode = mode;
	}

	TEXPREVIEW_MODE getTexturePreviewMode()
	{
		return texPreviewMode;
	}
}  // namespace render
#endif // ENGINE_DEBUG_RESOURCEVIEW

#if ENGINE_DEBUG_READBACK

#include <render/buffer.hpp>
#include <render/render_memview.hpp>
#include <system/logger.hpp>
#include <system/gui.hpp>
#include <algorithm>
#include <cstring>
#include <string>
#include <format>

static buffer* createReadBackBuffer(uint size, const char* debugName)
{
	return e_globBufAllocator.alloc(nullptr, size, 1, 0, buf::RESOURCE_READBACK, DXGI_FORMAT_UNKNOWN, 0, 0, 1, {}, nullptr, 0, 0, debugName);
}

void renderDebug::ensureDebugReadBackBuffer()
{
	if (debugReadBackBufferAttempted)
	{
		return;
	}
	debugReadBackBufferAttempted = true;
	debugReadBackBuffer = createReadBackBuffer(render::DEBUG_READBACK_BUFFER_SIZE, "debug readback buffer");
	if (debugReadBackBuffer == nullptr)
	{
		TC_LOG_ERROR("Failed to create shared debug readback buffer");
	}
}

buffer* renderDebug::getDebugReadBackBuffer()
{
	return debugReadBackBuffer;
}

#if ENGINE_DEBUG_MEMVIEW
void renderDebug::requestMemReadback(buffer* target, uint byteCount)
{
	memReadbackRequest = true;
	memReadbackTarget = target;
	memReadbackByteCount = byteCount;
}

const std::vector<unsigned char>& renderDebug::getMemReadbackData() const
{
	return memReadbackData;
}

uint renderDebug::getMemReadbackResultId() const
{
	return memReadbackResultId;
}

bool renderDebug::getMemReadbackFailed() const
{
	return memReadbackFailed;
}

void renderDebug::ensureMemLayouts()
{
	if (memLayoutsAttempted)
	{
		return;
	}
	memLayoutsAttempted = true;

	if (render::parseMemLayoutFile(render::MEMLAYOUT_DEFAULT_PATH, memLayouts))
	{
		TC_LOG_INFO(std::format("ensureMemLayouts: loaded {} layout(s) from {}",
			memLayouts.size(), render::MEMLAYOUT_DEFAULT_PATH).c_str());
	}
}
#endif // ENGINE_DEBUG_MEMVIEW

void renderDebug::update()
{
	ensureDebugReadBackBuffer();

#if ENGINE_DEBUG_MEMVIEW
	ensureMemLayouts();

#if ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW
	uint currentId = render::getSelectedResourceId();
	if (currentId != lastReadbackSelectionId)
	{
		lastReadbackSelectionId = currentId;
		if (currentId != ~0u && currentId < buf::getResourceDebugInfoCount() && buf::isBufferResource(currentId) && buf::getResourceOwner(currentId) != nullptr)
		{
			requestMemReadback(buf::getResourceOwner(currentId), (uint)(std::min)(buf::getResourceWidth(currentId), (UINT64)render::MEMVIEW_MAX_READBACK_BYTES));
		}
		else
		{
			memReadbackData.clear();
			memReadbackResultId = ~0u;
			memReadbackFailed = false;
		}
	}
#endif // ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW

	if (memReadbackRequest)
	{
		memReadbackRequest = false;
		memReadbackFailed = false;

		if (memReadbackTarget != nullptr && memReadbackByteCount > 0)
		{
			if (render::readbackBufferBytes(memReadbackTarget, memReadbackByteCount, memReadbackData))
			{
				memReadbackResultId = memReadbackTarget->getId();
			}
			else
			{
				memReadbackFailed = true;
				memReadbackResultId = ~0u;
			}
		}
		else
		{
			memReadbackFailed = true;
			memReadbackResultId = ~0u;
		}

		memReadbackTarget = nullptr;
	}
#endif // ENGINE_DEBUG_MEMVIEW
}

#if ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW
static int memviewOffsetBytes = 0;
static const char* const memviewWordColumns[8] = { "+0", "+4", "+8", "+12", "+16", "+20", "+24", "+28" };
static int memviewRawStrideBytes = 16;
static std::vector<uint> memviewRowFieldIndex;
static std::vector<uint> memviewRowLineIndex;
static int memviewRowMapForIndex = -1;

static void memviewExtractLine(const std::string& text, uint lineIndex, std::string& outLine)
{
	size_t start = 0;
	for (uint i = 0; i < lineIndex; ++i)
	{
		size_t newlinePos = text.find('\n', start);
		if (newlinePos == std::string::npos)
		{
			outLine.clear();
			return;
		}
		start = newlinePos + 1;
	}

	size_t end = text.find('\n', start);
	if (end == std::string::npos)
	{
		outLine = text.substr(start);
	}
	else
	{
		outLine = text.substr(start, end - start);
	}
}

static void guiTexturePreviewSection(uint id)
{
	ImGui::SeparatorText(buf::getResourceDisplayName(id));

	const char* reason = nullptr;
	if (!render::isTexturePreviewable(id, &reason))
	{
		ImGui::TextDisabled("(not previewable - %s)", reason);
		return;
	}

	D3D12_RESOURCE_DESC desc = buf::getResourceOwner(id)->getResource()->GetDesc();
	ImGui::Text("%llu x %u, %u mip(s), format %u", desc.Width, desc.Height, desc.MipLevels, (uint)desc.Format);

	int mip = render::getTexturePreviewMip();
	if (desc.MipLevels == 1)
	{
		ImGui::TextDisabled("(single mip)");
	}
	else
	{
		if (ImGui::SliderInt("Mip", &mip, 0, (int)desc.MipLevels - 1))
		{
			render::setTexturePreviewMip(mip);
		}
	}

	static const char* const modeNames[render::TEXPREVIEW_MODE_COUNT] =
		{ "Raw bits", "Octahedral normal", "Object ID color", "visID cluster color", "Red as grayscale" };
	render::TEXPREVIEW_MODE mode = render::getTexturePreviewMode();
	if (ImGui::BeginCombo("Interpret as", modeNames[mode]))
	{
		for (uint m = 0; m < render::TEXPREVIEW_MODE_COUNT; ++m)
		{
			ImGui::PushID((int)m);
			if (ImGui::Selectable(modeNames[m], mode == (render::TEXPREVIEW_MODE)m))
			{
				render::setTexturePreviewMode((render::TEXPREVIEW_MODE)m);
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	// Show mode applicability notes
	bool isInteger = render::isIntegerFormat(desc.Format);
	if ((mode >= 1 && mode <= 3) && !isInteger)
	{
		ImGui::TextDisabled("(mode applies to integer formats)");
	}
	else if (mode == 4 && isInteger)
	{
		ImGui::TextDisabled("(mode applies to float formats)");
	}

	static float texPreviewGain = 1.0f;
	ImGui::DragFloat("Gain", &texPreviewGain, 0.05f, 0.01f, 50.0f);

	render::texPreviewInfo info;
	if (render::getTexturePreviewInfo(info))
	{
		float availW = (std::min)(ImGui::GetContentRegionAvail().x, 512.0f);
		float drawW = (std::max)(availW, 1.0f);
		float drawH = drawW * ((float)info.height / (float)(std::max)(info.width, 1u));
		ImGui::Text("mip %u: %u x %u", info.mip, info.width, info.height);
		ImGui::Image((ImTextureID)info.handle.ptr, ImVec2(drawW, drawH), ImVec2(0, 0), ImVec2(info.u1, info.v1),
			ImVec4(texPreviewGain, texPreviewGain, texPreviewGain, 1),
			ImGui::GetStyleColorVec4(ImGuiCol_Border));
		if ((int)info.mip != render::getTexturePreviewMip())
		{
			ImGui::TextDisabled("(snapshot is of mip %u - press Update to recapture)", info.mip);
		}
		if (mode != render::texPreviewSnapshotMode)
		{
			ImGui::TextDisabled("(mode changed since snapshot - press Update to refresh)");
		}
	}
	else
	{
		ImGui::TextDisabled("(no snapshot - press Update to capture)");
	}
}

static void guiResourceCombo(uint selectedId)
{
	uint targetId = selectedId;
	bool targetValid = (targetId != ~0u) && (targetId < buf::getResourceDebugInfoCount());

	const char* preview = "(select a resource)";
	if (targetValid)
	{
		if (buf::isBufferResource(targetId))
		{
			preview = buf::getResourceDisplayName(targetId);
		}
		else if (buf::isTextureResource(targetId))
		{
			preview = buf::getResourceDisplayName(targetId);
		}
	}

	if (ImGui::BeginCombo("Resource", preview))
	{
		for (uint id = 0; id < buf::getResourceDebugInfoCount(); ++id)
		{
			bool isBuffer = buf::isBufferResource(id);
			bool isTexture = buf::isTextureResource(id);
			if (!isBuffer && !isTexture)
			{
				continue;
			}

			const char* tag = isBuffer ? "[buf]" : "[tex]";
			std::string label = std::string(buf::getResourceDisplayName(id)) + " " + tag;

			ImGui::PushID((int)id);
			if (ImGui::Selectable(label.c_str(), render::getSelectedResourceId() == id))
			{
				render::setSelectedResourceId(id);
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}
}

void renderDebug::guiUpdateButton(uint selectedId, bool selIsBuffer, bool selIsTexture)
{
	bool targetValid = (selectedId != ~0u) && (selectedId < buf::getResourceDebugInfoCount());

	ImGui::BeginDisabled(!targetValid);
	if (ImGui::Button("Update"))
	{
		if (targetValid)
		{
			if (selIsBuffer)
			{
				uint maxBytes = (uint)(std::min)(buf::getResourceWidth(selectedId), (UINT64)render::MEMVIEW_MAX_READBACK_BYTES);
				requestMemReadback(buf::getResourceOwner(selectedId), maxBytes);
			}
			else if (selIsTexture)
			{
				render::requestTexturePreviewCopy();
			}
		}
	}
	ImGui::EndDisabled();
}

void renderDebug::guiMemoryReadbackSetting()
{
	uint selectedId = render::getSelectedResourceId();
	bool selIsBuffer = (selectedId != ~0u) && (selectedId < buf::getResourceDebugInfoCount()) && buf::isBufferResource(selectedId);
	bool selIsTexture = (selectedId != ~0u) && (selectedId < buf::getResourceDebugInfoCount()) && buf::isTextureResource(selectedId);

	guiResourceCombo(selectedId);

	guiUpdateButton(selectedId, selIsBuffer, selIsTexture);

	if (selIsBuffer)
	{
		ImGui::Text("Size: %llu bytes", buf::getResourceWidth(selectedId));
	}

	if (selIsTexture)
	{
		guiTexturePreviewSection(selectedId);
	}
	else if (selIsBuffer)
	{
		if (selectedMemLayoutIndex >= (int)memLayouts.size())
	{
		selectedMemLayoutIndex = -1;
	}

	const char* layoutPreview = "(raw words)";
	if (selectedMemLayoutIndex >= 0)
	{
		layoutPreview = memLayouts[selectedMemLayoutIndex].name.c_str();
	}

	if (ImGui::BeginCombo("Default Layout", layoutPreview))
	{
		if (ImGui::Selectable("(raw words)", selectedMemLayoutIndex < 0))
		{
			selectedMemLayoutIndex = -1;
		}

		for (int i = 0; i < (int)memLayouts.size(); ++i)
		{
			ImGui::PushID(i);
			if (ImGui::Selectable(memLayouts[i].name.c_str(), selectedMemLayoutIndex == i))
			{
				selectedMemLayoutIndex = i;
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (memLayouts.empty())
	{
		ImGui::TextDisabled("(No layouts loaded - see log)");
	}

	if (getMemReadbackFailed())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Readback failed - see log");
	}

	const std::vector<unsigned char>& bytes = getMemReadbackData();
	if (bytes.empty())
	{
		ImGui::TextDisabled("(No data - select a buffer)");
	}
	else
	{
		ImGui::SeparatorText(buf::getResourceDisplayName(getMemReadbackResultId()));
		ImGui::Text("%zu bytes", bytes.size());

		ImGui::InputInt("Offset (bytes)", &memviewOffsetBytes);
		if (memviewOffsetBytes < 0)
		{
			memviewOffsetBytes = 0;
		}
		if (memviewOffsetBytes > (int)bytes.size())
		{
			memviewOffsetBytes = (int)bytes.size();
		}

		bool layoutSelected = (selectedMemLayoutIndex >= 0 && selectedMemLayoutIndex < (int)memLayouts.size());

		if (layoutSelected)
		{
			const render::memLayout& layout = memLayouts[selectedMemLayoutIndex];

			if (memviewRowMapForIndex != selectedMemLayoutIndex)
			{
				memviewRowMapForIndex = selectedMemLayoutIndex;
				memviewRowFieldIndex.clear();
				memviewRowLineIndex.clear();
				for (uint f = 0; f < (uint)layout.fields.size(); ++f)
				{
					uint lineCount = render::memFieldRowCount(layout.fields[f].type);
					for (uint line = 0; line < lineCount; ++line)
					{
						memviewRowFieldIndex.push_back(f);
						memviewRowLineIndex.push_back(line);
					}
				}
			}

			ImGui::Text("Layout '%s' (%u bytes)", layout.name.c_str(), layout.size);

			size_t blockBaseOffset = (size_t)memviewOffsetBytes;
			size_t remainingBytes = bytes.size() - blockBaseOffset;
			size_t blockStride = (layout.size > 0) ? (size_t)layout.size : 1;
			size_t totalBlocks = (remainingBytes + blockStride - 1) / blockStride;
			size_t rowsPerBlock = memviewRowFieldIndex.size() + 1;
			size_t maxBlocks = (size_t)INT_MAX / rowsPerBlock;
			if (totalBlocks > maxBlocks)
			{
				totalBlocks = maxBlocks;
			}
			size_t totalRows = totalBlocks * rowsPerBlock;

			if (totalBlocks == 0)
			{
				ImGui::TextDisabled("(Offset is at the end of the captured data - no blocks to show)");
			}
			else
			{
				ImGui::Text("%zu blocks (stride %zu bytes) from offset %d", totalBlocks, blockStride, memviewOffsetBytes);
			}

			if (totalBlocks > 0)
			{
				if (ImGui::BeginTable("MemviewFields", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
				{
					ImGui::TableSetupColumn("Field");
					ImGui::TableSetupColumn("Type");
					ImGui::TableSetupColumn("Value");
					ImGui::TableSetupScrollFreeze(1, 1);
					ImGui::TableHeadersRow();

					ImGuiListClipper clipper;
					clipper.Begin((int)totalRows);
					while (clipper.Step())
					{
						for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
						{
							size_t blockIndex = (size_t)row / rowsPerBlock;
							size_t rowInBlock = (size_t)row % rowsPerBlock;
							size_t blockOffset = blockBaseOffset + blockIndex * blockStride;

							ImGui::TableNextRow();

							if (rowInBlock == 0)
							{
								ImGui::TableSetColumnIndex(0);
								ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Block %zu  @ +%zu", blockIndex, blockOffset);
							}
							else
							{
								size_t mapIndex = rowInBlock - 1;
								uint fieldIndex = memviewRowFieldIndex[mapIndex];
								uint lineIndex = memviewRowLineIndex[mapIndex];
								const render::memLayoutField& field = layout.fields[fieldIndex];

								if (lineIndex == 0)
								{
									ImGui::TableSetColumnIndex(0);
									ImGui::Text("%s", field.name.c_str());

									ImGui::TableSetColumnIndex(1);
									ImGui::Text("%s", render::memFieldTypeName(field.type));
								}

								ImGui::TableSetColumnIndex(2);
								std::string valueText;
								render::MEMFIELD_DECODE_RESULT decodeResult = render::decodeMemField(
									bytes.data(), bytes.size(), blockOffset, field, valueText);

								if (decodeResult == render::MEMFIELD_DECODE_OUT_OF_BOUNDS)
								{
									if (lineIndex == 0)
									{
										ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Out of Bounds");
									}
								}
								else if (decodeResult == render::MEMFIELD_DECODE_INVALID_TYPE)
								{
									if (lineIndex == 0)
									{
										ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid Type");
									}
								}
								else
								{
									std::string lineText;
									memviewExtractLine(valueText, lineIndex, lineText);
									ImGui::Text("%s", lineText.c_str());
								}
							}
						}
					}

					ImGui::EndTable();
				}
			}
		}
		else
		{
			ImGui::InputInt("Stride (bytes)", &memviewRawStrideBytes);
			if (memviewRawStrideBytes < 4)
			{
				memviewRawStrideBytes = 4;
			}
			if (memviewRawStrideBytes > 32)
			{
				memviewRawStrideBytes = 32;
			}
			memviewRawStrideBytes = (memviewRawStrideBytes / 4) * 4;

			size_t offset = (size_t)memviewOffsetBytes;
			const unsigned char* sliceData = bytes.data() + offset;
			size_t sliceSize = bytes.size() - offset;
			size_t fullWordCount = sliceSize / 4;
			size_t trailingBytes = sliceSize % 4;
			uint wordsPerRow = (uint)memviewRawStrideBytes / 4u;

			if (ImGui::BeginTable("MemviewHex", (int)(wordsPerRow + 1u), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
			{
				ImGui::TableSetupColumn("Offset");
				for (uint c = 0; c < wordsPerRow; ++c)
				{
					ImGui::TableSetupColumn(memviewWordColumns[c]);
				}
				ImGui::TableSetupScrollFreeze(1, 1);
				ImGui::TableHeadersRow();

				uint rowCount = (uint)((fullWordCount + wordsPerRow - 1u) / wordsPerRow);
				ImGuiListClipper clipper;
				clipper.Begin((int)rowCount);
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%u", (uint)memviewOffsetBytes + (uint)row * (uint)memviewRawStrideBytes);
						for (uint c = 0; c < wordsPerRow; ++c)
						{
							uint wordIdx = (uint)row * wordsPerRow + c;
							ImGui::TableSetColumnIndex((int)(c + 1u));
							if (wordIdx < fullWordCount)
							{
								uint32_t value = 0;
								memcpy(&value, sliceData + (size_t)wordIdx * 4u, sizeof(value));
								ImGui::Text("%u", value);
							}
							else
							{
								ImGui::TextDisabled("--------");
							}
						}
					}
				}
				ImGui::EndTable();
			}

			if (trailingBytes > 0)
			{
				ImGui::TextDisabled("(%zu trailing byte%s dropped - not enough for a full uint32)", trailingBytes, trailingBytes == 1 ? "" : "s");
			}
		}
	}
	}
}
#endif // ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW

#endif // ENGINE_DEBUG_READBACK
