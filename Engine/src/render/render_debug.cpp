#include <render/render_debug.hpp>

#if ENGINE_DEBUG_READBACK

#include <render/buffer.hpp>
#include <render/render_memview.hpp>
#include <system/logger.hpp>
#include <system/gui.hpp>
#include <algorithm>
#include <cstring>

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
#endif // ENGINE_DEBUG_MEMVIEW

void renderDebug::update()
{
	ensureDebugReadBackBuffer();

#if ENGINE_DEBUG_MEMVIEW
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
#endif // ENGINE_DEBUG_MEMVIEW && ENGINE_DEBUG_RESOURCEVIEW

#endif // ENGINE_DEBUG_READBACK
