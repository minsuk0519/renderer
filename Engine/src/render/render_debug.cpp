#include <render/render_debug.hpp>

#if ENGINE_DEBUG_RESOURCEVIEW
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

void renderDebug::guiMemoryReadbackSetting()
{
	uint targetId = render::getSelectedResourceId();
	bool targetValid = (targetId != ~0u) && (targetId < buf::getResourceDebugInfoCount()) && buf::isBufferResource(targetId);

	const char* preview = targetValid ? buf::getResourceDisplayName(targetId) : "(select a buffer)";
	if (ImGui::BeginCombo("Resource", preview))
	{
		uint candidateCount = 0;
		for (uint id = 0; id < buf::getResourceDebugInfoCount(); ++id)
		{
			if (!buf::isBufferResource(id))
			{
				continue;
			}

			candidateCount++;
			ImGui::PushID((int)id);
			if (ImGui::Selectable(buf::getResourceDisplayName(id), render::getSelectedResourceId() == id))
			{
				render::setSelectedResourceId(id);
				targetId = id;
				targetValid = true;
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (buf::getResourceDebugInfoCount() > 0)
	{
		uint bufferCount = 0;
		for (uint id = 0; id < buf::getResourceDebugInfoCount(); ++id)
		{
			if (buf::isBufferResource(id))
			{
				bufferCount++;
			}
		}
		if (bufferCount == 0)
		{
			ImGui::TextDisabled("(No buffer resources available)");
		}
	}

	if (targetValid)
	{
		ImGui::Text("Size: %llu bytes", buf::getResourceWidth(targetId));
	}

	ImGui::BeginDisabled(!targetValid);
	if (ImGui::Button("Update"))
	{
		if (targetValid)
		{
			uint maxBytes = (uint)(std::min)(buf::getResourceWidth(targetId), (UINT64)render::MEMVIEW_MAX_READBACK_BYTES);
			requestMemReadback(buf::getResourceOwner(targetId), maxBytes);
		}
	}
	ImGui::EndDisabled();

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
#endif // ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW

#endif // ENGINE_DEBUG_READBACK
