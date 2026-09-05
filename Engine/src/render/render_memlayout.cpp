#include <render/render_memlayout.hpp>

#if ENGINE_DEBUG_MEMVIEW

#include <system/jsonhelper.hpp>
#include <system/logger.hpp>
#include <format>
#include <algorithm>
#include <cstring>

namespace render
{
	struct MemFieldTypeInfo
	{
		const char* jsonName;
		uint sizeBytes;
	};

	static constexpr MemFieldTypeInfo MEMFIELD_TYPE_TABLE[] = {
		{ "", 0 },               // MEMFIELD_UNKNOWN
		{ "float", 4 },          // MEMFIELD_FLOAT
		{ "float2", 8 },         // MEMFIELD_FLOAT2
		{ "float3", 12 },        // MEMFIELD_FLOAT3
		{ "float4", 16 },        // MEMFIELD_FLOAT4
		{ "int", 4 },            // MEMFIELD_INT
		{ "int2", 8 },           // MEMFIELD_INT2
		{ "int3", 12 },          // MEMFIELD_INT3
		{ "int4", 16 },          // MEMFIELD_INT4
		{ "uint", 4 },           // MEMFIELD_UINT
		{ "uint2", 8 },          // MEMFIELD_UINT2
		{ "uint3", 12 },         // MEMFIELD_UINT3
		{ "uint4", 16 },         // MEMFIELD_UINT4
		{ "ushort", 2 },         // MEMFIELD_USHORT
		{ "bool", 4 },           // MEMFIELD_BOOL
		{ "float4x4", 64 },      // MEMFIELD_FLOAT4X4
	};

	static_assert(sizeof(MEMFIELD_TYPE_TABLE) / sizeof(MEMFIELD_TYPE_TABLE[0]) == MEMFIELD_COUNT,
		"MEMFIELD_TYPE_TABLE must have exactly MEMFIELD_COUNT entries");

	uint memFieldTypeSize(MEMFIELD_TYPE type)
	{
		if (type >= MEMFIELD_COUNT)
		{
			return 0;
		}
		return MEMFIELD_TYPE_TABLE[type].sizeBytes;
	}

	const char* memFieldTypeName(MEMFIELD_TYPE type)
	{
		if (type >= MEMFIELD_COUNT)
		{
			return "";
		}
		return MEMFIELD_TYPE_TABLE[type].jsonName;
	}

	MEMFIELD_TYPE memFieldTypeFromString(const std::string& typeName)
	{
		for (uint i = 1; i < MEMFIELD_COUNT; ++i)
		{
			if (typeName == MEMFIELD_TYPE_TABLE[i].jsonName)
			{
				return static_cast<MEMFIELD_TYPE>(i);
			}
		}
		return MEMFIELD_UNKNOWN;
	}

	bool parseMemLayoutFile(const std::string& filePath, std::vector<memLayout>& outLayouts)
	{
		outLayouts.clear();

		if (filePath.empty())
		{
			TC_LOG_ERROR("parseMemLayoutFile: filePath is empty");
			return false;
		}

		std::vector<memLayoutJson> rawLayouts;
		if (!readJsonFile(rawLayouts, filePath))
		{
			return false;
		}

		if (rawLayouts.empty())
		{
			TC_LOG_ERROR(std::format("parseMemLayoutFile: {} contains no layouts", filePath).c_str());
			return false;
		}

		std::vector<memLayout> layouts;
		std::vector<std::string> acceptedLayoutNames;

		for (const auto& rawLayout : rawLayouts)
		{
			if (rawLayout.name.empty())
			{
				TC_LOG_ERROR(std::format("parseMemLayoutFile: {} layout has empty name", filePath).c_str());
				outLayouts.clear();
				return false;
			}

			if (std::find(acceptedLayoutNames.begin(), acceptedLayoutNames.end(), rawLayout.name) != acceptedLayoutNames.end())
			{
				TC_LOG_ERROR(std::format("parseMemLayoutFile: {} layout '{}' is a duplicate", filePath, rawLayout.name).c_str());
				outLayouts.clear();
				return false;
			}

			if (rawLayout.fields.empty())
			{
				TC_LOG_ERROR(std::format("parseMemLayoutFile: {} layout '{}' has no fields", filePath, rawLayout.name).c_str());
				outLayouts.clear();
				return false;
			}

			memLayout layout;
			layout.name = rawLayout.name;

			for (const auto& rawField : rawLayout.fields)
			{
				if (rawField.name.empty())
				{
					TC_LOG_ERROR(std::format("parseMemLayoutFile: {} layout '{}' has a field with empty name", filePath, rawLayout.name).c_str());
					outLayouts.clear();
					return false;
				}

				auto existingField = std::find_if(layout.fields.begin(), layout.fields.end(),
					[&rawField](const memLayoutField& f) { return f.name == rawField.name; });
				if (existingField != layout.fields.end())
				{
					TC_LOG_WARNING(std::format("parseMemLayoutFile: {} layout '{}' field '{}' is a duplicate", filePath, rawLayout.name, rawField.name).c_str());
				}

				MEMFIELD_TYPE fieldType = memFieldTypeFromString(rawField.type);
				if (fieldType == MEMFIELD_UNKNOWN)
				{
					TC_LOG_ERROR(std::format("parseMemLayoutFile: {} layout '{}' field '{}' has unrecognized type '{}'",
						filePath, rawLayout.name, rawField.name, rawField.type).c_str());
					outLayouts.clear();
					return false;
				}

				uint fieldSize = memFieldTypeSize(fieldType);

				if (rawField.offset > 0xFFFFFFFFu - fieldSize)
				{
					TC_LOG_ERROR(std::format("parseMemLayoutFile: {} layout '{}' field '{}' offset+size overflows",
						filePath, rawLayout.name, rawField.name).c_str());
					outLayouts.clear();
					return false;
				}

				memLayoutField field;
				field.name = rawField.name;
				field.type = fieldType;
				field.offset = rawField.offset;
				field.size = fieldSize;

				layout.fields.push_back(field);
				layout.size = (std::max)(layout.size, field.offset + field.size);
			}

			for (size_t i = 0; i < layout.fields.size(); ++i)
			{
				for (size_t j = i + 1; j < layout.fields.size(); ++j)
				{
					const auto& fieldA = layout.fields[i];
					const auto& fieldB = layout.fields[j];
					uint aEnd = fieldA.offset + fieldA.size;
					uint bEnd = fieldB.offset + fieldB.size;

					if (!(aEnd <= fieldB.offset || bEnd <= fieldA.offset))
					{
						TC_LOG_WARNING(std::format("parseMemLayoutFile: {} layout '{}' fields '{}' and '{}' overlap",
							filePath, rawLayout.name, fieldA.name, fieldB.name).c_str());
					}
				}
			}

			acceptedLayoutNames.push_back(rawLayout.name);
			layouts.push_back(layout);
		}

		outLayouts = std::move(layouts);
		return true;
	}

	const memLayout* findMemLayout(const std::vector<memLayout>& layouts, const std::string& name)
	{
		for (const auto& layout : layouts)
		{
			if (layout.name == name)
			{
				return &layout;
			}
		}
		return nullptr;
	}

	MEMFIELD_DECODE_RESULT decodeMemField(const unsigned char* data, size_t dataSize, size_t baseOffset,
										  const memLayoutField& field, std::string& outText)
	{
		outText.clear();

		if (field.type == MEMFIELD_UNKNOWN || field.type >= MEMFIELD_COUNT)
		{
			return MEMFIELD_DECODE_INVALID_TYPE;
		}

		if (data == nullptr || field.size == 0)
		{
			return MEMFIELD_DECODE_OUT_OF_BOUNDS;
		}

		size_t start = baseOffset + (size_t)field.offset;
		size_t end = start + (size_t)field.size;

		if (end > dataSize || end < start)
		{
			return MEMFIELD_DECODE_OUT_OF_BOUNDS;
		}

		switch (field.type)
		{
		case MEMFIELD_FLOAT:
		{
			float v = 0.0f;
			memcpy(&v, data + start, sizeof(v));
			outText = std::format("{}", v);
			break;
		}
		case MEMFIELD_FLOAT2:
		{
			float v[2] = { 0.0f, 0.0f };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}", v[0], v[1]);
			break;
		}
		case MEMFIELD_FLOAT3:
		{
			float v[3] = { 0.0f, 0.0f, 0.0f };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}, {}", v[0], v[1], v[2]);
			break;
		}
		case MEMFIELD_FLOAT4:
		{
			float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}, {}, {}", v[0], v[1], v[2], v[3]);
			break;
		}
		case MEMFIELD_INT:
		{
			int32_t v = 0;
			memcpy(&v, data + start, sizeof(v));
			outText = std::format("{}", v);
			break;
		}
		case MEMFIELD_INT2:
		{
			int32_t v[2] = { 0, 0 };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}", v[0], v[1]);
			break;
		}
		case MEMFIELD_INT3:
		{
			int32_t v[3] = { 0, 0, 0 };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}, {}", v[0], v[1], v[2]);
			break;
		}
		case MEMFIELD_INT4:
		{
			int32_t v[4] = { 0, 0, 0, 0 };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}, {}, {}", v[0], v[1], v[2], v[3]);
			break;
		}
		case MEMFIELD_UINT:
		{
			uint32_t v = 0;
			memcpy(&v, data + start, sizeof(v));
			outText = std::format("{}", v);
			break;
		}
		case MEMFIELD_UINT2:
		{
			uint32_t v[2] = { 0, 0 };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}", v[0], v[1]);
			break;
		}
		case MEMFIELD_UINT3:
		{
			uint32_t v[3] = { 0, 0, 0 };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}, {}", v[0], v[1], v[2]);
			break;
		}
		case MEMFIELD_UINT4:
		{
			uint32_t v[4] = { 0, 0, 0, 0 };
			memcpy(v, data + start, sizeof(v));
			outText = std::format("{}, {}, {}, {}", v[0], v[1], v[2], v[3]);
			break;
		}
		case MEMFIELD_USHORT:
		{
			uint16_t v = 0;
			memcpy(&v, data + start, sizeof(v));
			outText = std::format("{}", v);
			break;
		}
		case MEMFIELD_BOOL:
		{
			uint32_t v = 0;
			memcpy(&v, data + start, sizeof(v));
			outText = v != 0 ? "true" : "false";
			break;
		}
		case MEMFIELD_FLOAT4X4:
		{
			float m[16] = { 0.0f };
			memcpy(m, data + start, sizeof(m));
			outText = std::format("{}, {}, {}, {}\n{}, {}, {}, {}\n{}, {}, {}, {}\n{}, {}, {}, {}",
				m[0], m[1], m[2], m[3],
				m[4], m[5], m[6], m[7],
				m[8], m[9], m[10], m[11],
				m[12], m[13], m[14], m[15]);
			break;
		}
		default:
			return MEMFIELD_DECODE_INVALID_TYPE;
		}

		return MEMFIELD_DECODE_OK;
	}

}  // namespace render

#endif // ENGINE_DEBUG_MEMVIEW
