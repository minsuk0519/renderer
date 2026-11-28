#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#ifndef USE_WAVE64
#define USE_WAVE64 0
#endif

namespace sgh
{
	constexpr uint32_t CULL_FLAG_CAST_SHADOW = 1u;

	struct InstanceIn
	{
		uint64_t key = 0;
		double worldMin[3] = {};
		double worldMax[3] = {};
		uint32_t cullFlags = CULL_FLAG_CAST_SHADOW;
	};

	struct BuildParams
	{
		uint32_t quantShift = 8;
		uint32_t bvhWidth = 8;
		uint32_t threadCount = 1;
		uint32_t mergeKNum = 2;
		uint32_t mergeKDen = 1;
		bool crossCheckBrute = false;
		uint32_t maxPromoteLevels = 2;
	};

#if USE_WAVE64
	constexpr uint32_t GROUP_MAX = 64;
#else
	constexpr uint32_t GROUP_MAX = 32;
#endif
	constexpr uint32_t MAX_PROMOTE_LEVELS = 8;
	constexpr uint32_t MERGE_SIZE_RATIO = 2;
	constexpr uint32_t GROWTH_BUCKETS = 6;
	constexpr uint32_t CHILD_LEAF_BIT = 0x80000000u;
	constexpr uint32_t CHILD_INST_BIT = 0x40000000u;
	constexpr uint32_t CHILD_EMPTY = 0xFFFFFFFFu;
	constexpr uint32_t NODE_WIDTH = 8;
	constexpr uint32_t FILE_VERSION = 4;
	constexpr uint32_t SECTION_COUNT = 9;
	constexpr uint32_t HEADER_SIZE = 96;
	constexpr uint32_t SECTION_ENTRY_SIZE = 24;

	enum class SectionId : uint32_t
	{
		Nodes,
		InstanceBoxes,
		InstanceCullFlags,
		GroupSlotBegin,
		LevelExp,
		GroupLevel,
		SlotInstance,
		SlotKey,
		KeyToSlot
	};

	struct FloatBox
	{
		float mn[3] = {};
		float mx[3] = {};
	};
	static_assert(sizeof(FloatBox) == 24);

	struct alignas(64) Node
	{
		float minX[NODE_WIDTH] = {};
		float minY[NODE_WIDTH] = {};
		float minZ[NODE_WIDTH] = {};
		float maxX[NODE_WIDTH] = {};
		float maxY[NODE_WIDTH] = {};
		float maxZ[NODE_WIDTH] = {};
		uint32_t childRef[NODE_WIDTH] = {};
		uint32_t groupBegin[NODE_WIDTH + 1] = {};
		uint32_t leafBegin[NODE_WIDTH + 1] = {};
		uint32_t reserved[6] = {};
	};
	static_assert(sizeof(Node) == 320);
	static_assert(alignof(Node) == 64);
	static_assert(offsetof(Node, minX) == 0);
	static_assert(offsetof(Node, minY) == 32);
	static_assert(offsetof(Node, minZ) == 64);
	static_assert(offsetof(Node, maxX) == 96);
	static_assert(offsetof(Node, maxY) == 128);
	static_assert(offsetof(Node, maxZ) == 160);
	static_assert(offsetof(Node, childRef) == 192);
	static_assert(offsetof(Node, groupBegin) == 224);
	static_assert(offsetof(Node, leafBegin) == 260);
	static_assert(offsetof(Node, reserved) == 296);

	struct KeySlot
	{
		uint64_t key = 0;
		uint32_t slot = 0;
		uint32_t zero = 0;
	};
	static_assert(sizeof(KeySlot) == 16);
	static_assert(std::has_unique_object_representations_v<KeySlot>);

	struct SectionEntry
	{
		uint64_t offset = 0;
		uint64_t size = 0;
		uint32_t crc = 0;
		uint32_t alignment = 0;
	};
	static_assert(sizeof(SectionEntry) == SECTION_ENTRY_SIZE);
	static_assert(std::has_unique_object_representations_v<SectionEntry>);

	struct File
	{
		uint32_t version = 0;
		uint64_t configHash = 0;
		uint32_t levelId = 0;
		uint32_t quantShift = 0;
		int64_t origin[3] = {};
		uint32_t levelCount = 0;
		uint32_t instanceCount = 0;
		uint32_t groupCount = 0;
		uint32_t nodeCount = 0;
		uint32_t rootRef = 0;
		FloatBox rootBox;
		SectionEntry sections[SECTION_COUNT] = {};
		std::vector<Node> nodes;
		std::vector<FloatBox> instanceBoxes;
		std::vector<uint32_t> cullFlags;
		std::vector<uint32_t> groupSlotBegin;
		std::vector<uint32_t> levelExp;
		std::vector<uint32_t> groupLevel;
		std::vector<uint32_t> slotInstance;
		std::vector<uint64_t> slotKey;
		std::vector<KeySlot> keyToSlot;
	};

	struct ReadResult
	{
		bool ok = false;
		std::string error;
		File file;
	};

	struct Plane
	{
		float nx = 0.0f;
		float ny = 0.0f;
		float nz = 0.0f;
		float d = 0.0f;
	};

	struct IndexRange
	{
		uint32_t begin = 0;
		uint32_t end = 0;
	};

	enum class CullPath : uint8_t
	{
		Auto,
		Scalar,
		Avx2
	};

	struct LaneClass
	{
		uint32_t outsideBits = 0;
		uint32_t childPlaneMask[NODE_WIDTH] = {};
	};

	struct CullResult
	{
		bool ok = false;
		std::string error;
		std::vector<IndexRange> groupRanges;
		std::vector<IndexRange> leafRanges;
		uint32_t nodesTested = 0;
	};

	struct RoundStats
	{
		uint32_t mergedPool = 0;
		uint32_t unmergedPool = 0;
		uint32_t blockCount = 0;
		uint32_t maxBlockPop = 0;
		uint32_t merges = 0;
		uint32_t finalized = 0;
		uint64_t aabbTests = 0;
		uint64_t aabbPasses = 0;
		uint64_t pairEvals = 0;
		uint64_t rejectCount = 0;
		uint64_t rejectRatio = 0;
		uint64_t rejectGrowth = 0;
		uint64_t growthHist[GROWTH_BUCKETS] = {};
		uint64_t growthHistKTested[GROWTH_BUCKETS] = {};
	};

	struct LevelStats
	{
		uint32_t exp = 0;
		uint32_t groupCount = 0;
		uint32_t leafCount = 0;
		double areaSum = 0.0;
		double groupAreaSum = 0.0;
		uint32_t fill[GROUP_MAX + 1] = {};
		std::vector<RoundStats> roundStats;
		double mergeMs = 0.0;
	};

	struct BuildStats
	{
		uint32_t levelCount = 0;
		uint32_t instanceCount = 0;
		uint32_t groupCount = 0;
		uint32_t nodeCount = 0;
		uint32_t threadCount = 0;
		double sweepMs = 0.0;
		double bvhMs = 0.0;
		double totalMs = 0.0;
		double laneAreaQuantized = 0.0;
		double laneAreaFinal = 0.0;
		double instanceAreaQuantized = 0.0;
		double instanceAreaFinal = 0.0;
		std::vector<LevelStats> levels;
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
	ReadResult readFile(const std::vector<uint8_t>& bytes);
	LaneClass classifyNodeScalar(const Node& node, const Plane (&planes)[6], uint32_t activeMask);
	CullResult cullFrustum(const File& file, const Plane (&planes)[6], CullPath path = CullPath::Auto);
}
