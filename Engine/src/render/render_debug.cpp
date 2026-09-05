#include <render/render_debug.hpp>

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
static const char* const memviewWordColumns[4] = { "+0", "+4", "+8", "+12" };
static int memviewLayoutStrideBytes = 0;
static int memviewLayoutBlockCount = 64;
static int memviewLayoutStrideForIndex = -1;

void renderDebug::guiMemoryReadbackSetting()
{
	uint targetId = buf::getSelectedResourceId();
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
			if (ImGui::Selectable(buf::getResourceDisplayName(id), buf::getSelectedResourceId() == id))
			{
				buf::setSelectedResourceId(id);
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
	if (ImGui::Button("Readback"))
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
		ImGui::TextDisabled("(No data - press Readback)");
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

			if (memviewLayoutStrideForIndex != selectedMemLayoutIndex)
			{
				memviewLayoutStrideForIndex = selectedMemLayoutIndex;
				memviewLayoutStrideBytes = (int)layout.size;
			}

			ImGui::Text("Layout '%s' (%u bytes)", layout.name.c_str(), layout.size);

			ImGui::InputInt("Stride (bytes)", &memviewLayoutStrideBytes);
			if (memviewLayoutStrideBytes < 1)
			{
				memviewLayoutStrideBytes = 1;
			}

			ImGui::InputInt("Blocks to show", &memviewLayoutBlockCount);
			if (memviewLayoutBlockCount < 1)
			{
				memviewLayoutBlockCount = 1;
			}
			if (memviewLayoutBlockCount > 1024)
			{
				memviewLayoutBlockCount = 1024;
			}

			size_t blockBaseOffset = (size_t)memviewOffsetBytes;
			size_t remainingBytes = bytes.size() - blockBaseOffset;
			size_t blockStride = (size_t)memviewLayoutStrideBytes;
			size_t totalBlocks = (remainingBytes + blockStride - 1) / blockStride;
			size_t shownBlocks = (std::min)(totalBlocks, (size_t)memviewLayoutBlockCount);

			if (totalBlocks == 0)
			{
				ImGui::TextDisabled("(Offset is at the end of the captured data - no blocks to show)");
			}
			else
			{
				ImGui::Text("showing blocks 0-%zu of %zu (stride %d bytes) from offset %d",
					shownBlocks - 1, totalBlocks, memviewLayoutStrideBytes, memviewOffsetBytes);
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

					for (size_t blockIndex = 0; blockIndex < shownBlocks; ++blockIndex)
					{
						size_t blockOffset = blockBaseOffset + blockIndex * blockStride;

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Block %zu  @ +%zu", blockIndex, blockOffset);

						for (const render::memLayoutField& field : layout.fields)
						{
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::Text("%s", field.name.c_str());

							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%s", render::memFieldTypeName(field.type));

							ImGui::TableSetColumnIndex(2);
							std::string valueText;
							render::MEMFIELD_DECODE_RESULT decodeResult = render::decodeMemField(
								bytes.data(), bytes.size(), blockOffset, field, valueText);

							if (decodeResult == render::MEMFIELD_DECODE_OUT_OF_BOUNDS)
							{
								ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Out of Bounds");
							}
							else if (decodeResult == render::MEMFIELD_DECODE_INVALID_TYPE)
							{
								ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid Type");
							}
							else
							{
								ImGui::Text("%s", valueText.c_str());
							}
						}
					}

					ImGui::EndTable();
				}
			}
		}
		else
		{
			size_t offset = (size_t)memviewOffsetBytes;
			const unsigned char* sliceData = bytes.data() + offset;
			size_t sliceSize = bytes.size() - offset;
			size_t fullWordCount = sliceSize / 4;
			size_t trailingBytes = sliceSize % 4;

			if (ImGui::BeginTable("MemviewHex", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
			{
				ImGui::TableSetupColumn("Offset");
				for (uint c = 0; c < 4u; ++c)
				{
					ImGui::TableSetupColumn(memviewWordColumns[c]);
				}
				ImGui::TableSetupScrollFreeze(1, 1);
				ImGui::TableHeadersRow();

				uint rowCount = (uint)((fullWordCount + 3u) / 4u);
				ImGuiListClipper clipper;
				clipper.Begin((int)rowCount);
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%u", (uint)memviewOffsetBytes + (uint)row * 16u);
						for (uint c = 0; c < 4u; ++c)
						{
							uint wordIdx = (uint)row * 4u + c;
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
