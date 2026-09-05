#pragma once

#include <system/defines.hpp>
#include <string>
#include <vector>
#include <cstddef>

#if ENGINE_DEBUG_MEMVIEW

namespace render
{
	enum MEMFIELD_TYPE : uint
	{
		MEMFIELD_UNKNOWN = 0,
		MEMFIELD_FLOAT,
		MEMFIELD_FLOAT2,
		MEMFIELD_FLOAT3,
		MEMFIELD_FLOAT4,
		MEMFIELD_INT,
		MEMFIELD_INT2,
		MEMFIELD_INT3,
		MEMFIELD_INT4,
		MEMFIELD_UINT,
		MEMFIELD_UINT2,
		MEMFIELD_UINT3,
		MEMFIELD_UINT4,
		MEMFIELD_USHORT,
		MEMFIELD_BOOL,
		MEMFIELD_FLOAT4X4,
		MEMFIELD_COUNT,
	};

	constexpr const char* MEMLAYOUT_DEFAULT_PATH = "data/layout.json";

	struct memLayoutField
	{
		std::string name;
		MEMFIELD_TYPE type = MEMFIELD_UNKNOWN;
		uint offset = 0;
		uint size = 0;
	};

	struct memLayout
	{
		std::string name;
		uint size = 0;
		std::vector<memLayoutField> fields;
	};

	uint memFieldTypeSize(MEMFIELD_TYPE type);
	const char* memFieldTypeName(MEMFIELD_TYPE type);
	MEMFIELD_TYPE memFieldTypeFromString(const std::string& typeName);

	bool parseMemLayoutFile(const std::string& filePath, std::vector<memLayout>& outLayouts);
	const memLayout* findMemLayout(const std::vector<memLayout>& layouts, const std::string& name);

	enum MEMFIELD_DECODE_RESULT : uint
	{
		MEMFIELD_DECODE_OK = 0,
		MEMFIELD_DECODE_OUT_OF_BOUNDS,
		MEMFIELD_DECODE_INVALID_TYPE,
	};

	MEMFIELD_DECODE_RESULT decodeMemField(const unsigned char* data, size_t dataSize, size_t baseOffset,
										  const memLayoutField& field, std::string& outText);

}  // namespace render

#endif // ENGINE_DEBUG_MEMVIEW
