#pragma once
#include <cstdint>
#include <string>
#include <vector>

#ifndef USE_WAVE64
#define USE_WAVE64 0
#endif

namespace sgh
{
	struct InstanceIn
	{
		uint64_t key = 0;
		double worldMin[3] = {};
		double worldMax[3] = {};
	};

	enum class SfcCurve : uint32_t { Morton = 0, Hilbert = 1 };

	struct BuildParams
	{
		uint32_t quantShift = 8;
		uint32_t bvhWidth = 8;
		uint32_t threadCount = 1;
		uint32_t mergeKNum = 2;
		uint32_t mergeKDen = 1;
		uint32_t mergeWindow = 16;
		SfcCurve curve = SfcCurve::Morton;
	};

	constexpr uint8_t GROUP_OUTLIER     = 1u << 1;
	constexpr uint8_t GROUP_MERGED      = 1u << 2;

#if USE_WAVE64
	constexpr uint32_t GROUP_MAX = 64;
#else
	constexpr uint32_t GROUP_MAX = 32;
#endif
	constexpr uint32_t PROMOTE_SIZE_X100 = 175;
	constexpr uint32_t MERGE_SIZE_RATIO = 2;
	constexpr uint32_t CHILD_LEAF_BIT = 0x80000000u;
	constexpr uint32_t CHILD_INST_BIT = 0x40000000u;
	constexpr uint32_t FILE_VERSION = 2;

	struct BuildStats
	{
		uint32_t levelCount = 0;
		uint32_t instanceCount = 0;
		uint32_t groupCount = 0;
		uint32_t nodeCount = 0;
	};

	struct BuildResult
	{
		bool ok = false;
		std::string error;
		std::vector<uint8_t> bytes;
		uint64_t contentHash = 0;
		BuildStats stats;
	};

	using AssertHandler = void(*)(const char* expr, const char* file, int line);

	void setAssertHandler(AssertHandler handler);
	uint64_t hashBytes(const std::vector<uint8_t>& bytes);
	BuildResult build(const std::vector<InstanceIn>& instances, uint32_t levelId, const BuildParams& params);
}
