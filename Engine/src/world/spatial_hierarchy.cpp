#include <world/spatial_hierarchy.hpp>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <intrin.h>
#include <limits>
#include <numeric>
#include <thread>
#include <vector>

static_assert(std::endian::native == std::endian::little);

namespace sgh
{
	static_assert(GROUP_MAX >= 1 && GROUP_MAX <= 65535);
	static_assert(NODE_WIDTH == 8);

	namespace
	{
		void defaultAssertHandler(const char* expr, const char* file, int line)
		{
			std::fprintf(stderr, "[sgh] invariant failed: %s (%s:%d)\n", expr, file, line);
			std::abort();
		}

		AssertHandler g_assertHandler = &defaultAssertHandler;

#define SGH_VERIFY(cond) do { if (!(cond)) { g_assertHandler(#cond, __FILE__, __LINE__); } } while (0)

		constexpr uint32_t PAR_CHUNK = 64;

		template <typename Fn>
		void runParallelJob(uint32_t count, uint32_t threadCount, Fn fn)
		{
			if (count == 0)
			{
				return;
			}

			const uint32_t chunkCount = (count + PAR_CHUNK - 1) / PAR_CHUNK;
			const uint32_t t = (std::max)(1u, (std::min)(threadCount, chunkCount));

			auto worker = [&](uint32_t tid)
			{
				for (uint32_t c = tid; c < chunkCount; c += t)
				{
					const uint32_t b = c * PAR_CHUNK;
					fn(b, (std::min)(b + PAR_CHUNK, count));
				}
			};

			if (t <= 1)
			{
				worker(0);
				return;
			}

			std::vector<std::thread> threads;
			threads.reserve(t);
			for (uint32_t i = 0; i < t; ++i)
			{
				threads.emplace_back(worker, i);
			}
			for (std::thread& th : threads)
			{
				th.join();
			}
		}

		struct IAabb
		{
			int32_t mn[3];
			int32_t mx[3];
		};

		IAabb unionBox(const IAabb& a, const IAabb& b)
		{
			IAabb r{};
			for (int i = 0; i < 3; ++i)
			{
				r.mn[i] = (std::min)(a.mn[i], b.mn[i]);
				r.mx[i] = (std::max)(a.mx[i], b.mx[i]);
			}
			return r;
		}

		bool containsBox(const IAabb& outer, const IAabb& inner)
		{
			for (int a = 0; a < 3; ++a)
			{
				if (outer.mn[a] > inner.mn[a] || outer.mx[a] < inner.mx[a])
				{
					return false;
				}
			}
			return true;
		}

		constexpr uint32_t WORLD_EXTENT_LOG2 = 30;
		constexpr uint32_t MERGE_K_MAX = 3;
		static_assert(MERGE_K_MAX <= (std::numeric_limits<uint64_t>::max)() / (3ull << (2 * WORLD_EXTENT_LOG2)));
		static_assert(5ull <= (std::numeric_limits<uint64_t>::max)() / (3ull << (2 * WORLD_EXTENT_LOG2)));
		static_assert(WORLD_EXTENT_LOG2 + MAX_PROMOTE_LEVELS + 1 < 62);

		uint64_t surfaceArea(const IAabb& b)
		{
			const uint64_t dx = (std::max)(uint64_t{ 1 }, static_cast<uint64_t>(static_cast<int64_t>(b.mx[0]) - b.mn[0]));
			const uint64_t dy = (std::max)(uint64_t{ 1 }, static_cast<uint64_t>(static_cast<int64_t>(b.mx[1]) - b.mn[1]));
			const uint64_t dz = (std::max)(uint64_t{ 1 }, static_cast<uint64_t>(static_cast<int64_t>(b.mx[2]) - b.mn[2]));
			return dx * dy + dy * dz + dz * dx;
		}

		int64_t longestAxis(const IAabb& b)
		{
			int64_t longest = 1;
			for (int a = 0; a < 3; ++a)
			{
				longest = (std::max)(longest, static_cast<int64_t>(b.mx[a]) - b.mn[a]);
			}
			return longest;
		}

		int64_t floorToPow2Multiple(int64_t v, uint32_t s)
		{
			SGH_VERIFY(s < 62);
			if (s >= 62)
			{
				return 0;
			}
			if (v >= 0)
			{
				return (v >> s) << s;
			}
			const int64_t roundUpBias = (int64_t{ 1 } << s) - 1;
			return -(((-v + roundUpBias) >> s) << s);
		}

		uint64_t part1by2(uint64_t v)
		{
			v &= 0x1fffffull;
			v = (v | (v << 32)) & 0x1f00000000ffffull;
			v = (v | (v << 16)) & 0x1f0000ff0000ffull;
			v = (v | (v << 8))  & 0x100f00f00f00f00full;
			v = (v | (v << 4))  & 0x10c30c30c30c30c3ull;
			v = (v | (v << 2))  & 0x1249249249249249ull;
			return v;
		}

		uint64_t morton3(uint64_t x, uint64_t y, uint64_t z)
		{
			return part1by2(x) | (part1by2(y) << 1) | (part1by2(z) << 2);
		}

		struct MortonKey
		{
			uint64_t hi = 0;
			uint64_t lo = 0;

			auto operator<=>(const MortonKey&) const = default;
		};

		MortonKey interleave96(uint32_t x, uint32_t y, uint32_t z)
		{
			constexpr uint64_t LOW_MASK = 0x1fffffull;
			const uint64_t lowPart = morton3(x & LOW_MASK, y & LOW_MASK, z & LOW_MASK);
			const uint64_t highPart = morton3(x >> 21, y >> 21, z >> 21);
			MortonKey key;
			key.lo = lowPart | (highPart << 63);
			key.hi = highPart >> 1;
			return key;
		}

		uint64_t compact1by2(uint64_t v)
		{
			v &= 0x1249249249249249ull;
			v = (v | (v >> 2)) & 0x10c30c30c30c30c3ull;
			v = (v | (v >> 4)) & 0x100f00f00f00f00full;
			v = (v | (v >> 8)) & 0x1f0000ff0000ffull;
			v = (v | (v >> 16)) & 0x1f00000000ffffull;
			v = (v | (v >> 32)) & 0x1fffffull;
			return v;
		}

		void deinterleave96(const MortonKey& k, uint32_t out[3])
		{
			constexpr uint64_t LOW_PART_MASK = (1ull << 63) - 1;
			const uint64_t lowPart = k.lo & LOW_PART_MASK;
			const uint64_t highPart = (k.hi << 1) | (k.lo >> 63);
			for (int a = 0; a < 3; ++a)
			{
				const uint64_t lowAxis = compact1by2(lowPart >> a);
				const uint64_t highAxis = compact1by2(highPart >> a);
				out[a] = static_cast<uint32_t>(lowAxis | (highAxis << 21));
			}
		}

		MortonKey shr128(const MortonKey& k, uint32_t bits)
		{
			SGH_VERIFY(bits < 64);
			if (bits == 0)
			{
				return k;
			}
			MortonKey r;
			r.lo = (k.lo >> bits) | (k.hi << (64 - bits));
			r.hi = k.hi >> bits;
			return r;
		}

		MortonKey absDiff128(const MortonKey& a, const MortonKey& b)
		{
			const MortonKey& larger = (a < b) ? b : a;
			const MortonKey& smaller = (a < b) ? a : b;
			MortonKey r;
			r.lo = larger.lo - smaller.lo;
			r.hi = larger.hi - smaller.hi - ((larger.lo < smaller.lo) ? 1ull : 0ull);
			return r;
		}

		uint32_t ceilLog2(int64_t d)
		{
			return d <= 1 ? 0u : static_cast<uint32_t>(std::bit_width(static_cast<uint64_t>(d - 1)));
		}

		float roundDown(double exact)
		{
			float f = static_cast<float>(exact);
			if (static_cast<double>(f) > exact)
			{
				f = std::nextafter(f, -std::numeric_limits<float>::infinity());
			}
			return f;
		}

		float roundUp(double exact)
		{
			float f = static_cast<float>(exact);
			if (static_cast<double>(f) < exact)
			{
				f = std::nextafter(f, std::numeric_limits<float>::infinity());
			}
			return f;
		}

		float toFloatDown(int64_t q, uint32_t quantShift)
		{
			return roundDown(std::ldexp(static_cast<double>(q), -static_cast<int>(quantShift)));
		}

		float toFloatUp(int64_t q, uint32_t quantShift)
		{
			return roundUp(std::ldexp(static_cast<double>(q), -static_cast<int>(quantShift)));
		}

		uint64_t fnv1a64(const uint8_t* data, size_t size, uint64_t seed = 0xcbf29ce484222325ull)
		{
			uint64_t h = seed;
			for (size_t i = 0; i < size; ++i)
			{
				h ^= static_cast<uint64_t>(data[i]);
				h *= 0x100000001b3ull;
			}
			return h;
		}

		uint32_t crc32(const uint8_t* data, size_t size)
		{
			uint32_t crc = 0xFFFFFFFFu;
			for (size_t i = 0; i < size; ++i)
			{
				crc ^= static_cast<uint32_t>(data[i]);
				for (int b = 0; b < 8; ++b)
				{
					const uint32_t mask = 0u - (crc & 1u);
					crc = (crc >> 1) ^ (0xEDB88320u & mask);
				}
			}
			return crc ^ 0xFFFFFFFFu;
		}

		struct ByteWriter
		{
			std::vector<uint8_t> bytes;

			template <typename T>
			void put(const T& v)
			{
				const size_t o = bytes.size();
				bytes.resize(o + sizeof(T));
				std::memcpy(bytes.data() + o, &v, sizeof(T));
			}

			void padTo(size_t align)
			{
				while (bytes.size() % align != 0)
				{
					bytes.push_back(0);
				}
			}
		};

		bool isNodeRef(uint32_t ref)
		{
			return (ref & (CHILD_LEAF_BIT | CHILD_INST_BIT)) == 0;
		}

		bool isValidChildRef(uint32_t ref, uint32_t parent, uint32_t nodeCount, uint32_t groupCount, uint32_t instanceCount)
		{
			if ((ref & CHILD_LEAF_BIT) != 0)
			{
				return (ref & ~CHILD_LEAF_BIT) < groupCount;
			}
			if ((ref & CHILD_INST_BIT) != 0)
			{
				return (ref & ~CHILD_INST_BIT) < instanceCount;
			}
			return ref > parent && ref < nodeCount;
		}

		FloatBox laneBox(const Node& node, uint32_t lane)
		{
			return FloatBox{
				{ node.minX[lane], node.minY[lane], node.minZ[lane] },
				{ node.maxX[lane], node.maxY[lane], node.maxZ[lane] }
			};
		}

		void setLaneBox(Node& node, uint32_t lane, const FloatBox& box)
		{
			node.minX[lane] = box.mn[0];
			node.minY[lane] = box.mn[1];
			node.minZ[lane] = box.mn[2];
			node.maxX[lane] = box.mx[0];
			node.maxY[lane] = box.mx[1];
			node.maxZ[lane] = box.mx[2];
		}

		FloatBox emptyLaneBox()
		{
			constexpr float INF = std::numeric_limits<float>::infinity();
			return FloatBox{ { INF, INF, INF }, { -INF, -INF, -INF } };
		}

		bool isEmptyLaneBox(const FloatBox& box)
		{
			constexpr float INF = std::numeric_limits<float>::infinity();
			for (int a = 0; a < 3; ++a)
			{
				if (box.mn[a] != INF || box.mx[a] != -INF)
				{
					return false;
				}
			}
			return true;
		}

		bool isFiniteOrderedBox(const FloatBox& box)
		{
			for (int a = 0; a < 3; ++a)
			{
				if (!std::isfinite(box.mn[a]) || !std::isfinite(box.mx[a]) || box.mn[a] > box.mx[a])
				{
					return false;
				}
			}
			return true;
		}

		bool containsFloatBox(const FloatBox& outer, const FloatBox& inner)
		{
			for (int a = 0; a < 3; ++a)
			{
				if (!(outer.mn[a] <= inner.mn[a]) || !(outer.mx[a] >= inner.mx[a]))
				{
					return false;
				}
			}
			return true;
		}

		uint32_t sectionAlignment(SectionId id)
		{
			return id == SectionId::Nodes ? 64u : 16u;
		}

		struct InstRec
		{
			uint64_t key = 0;
			IAabb box{};
			uint32_t e = 0;
			uint32_t level = 0;
			uint32_t cullFlags = 0;
			double srcMin[3] = {};
			double srcMax[3] = {};
		};

		FloatBox sourceFloatBox(const InstRec& rec)
		{
			FloatBox box;
			for (int a = 0; a < 3; ++a)
			{
				box.mn[a] = roundDown(rec.srcMin[a]);
				box.mx[a] = roundUp(rec.srcMax[a]);
			}
			return box;
		}

		FloatBox unionFloatBox(const FloatBox& a, const FloatBox& b)
		{
			FloatBox r;
			for (int i = 0; i < 3; ++i)
			{
				r.mn[i] = (std::min)(a.mn[i], b.mn[i]);
				r.mx[i] = (std::max)(a.mx[i], b.mx[i]);
			}
			return r;
		}

		bool containsSource(const FloatBox& outer, const InstRec& rec)
		{
			for (int a = 0; a < 3; ++a)
			{
				if (!(static_cast<double>(outer.mn[a]) <= rec.srcMin[a]) || !(static_cast<double>(outer.mx[a]) >= rec.srcMax[a]))
				{
					return false;
				}
			}
			return true;
		}

		double floatBoxArea(const FloatBox& box)
		{
			const double dx = static_cast<double>(box.mx[0]) - static_cast<double>(box.mn[0]);
			const double dy = static_cast<double>(box.mx[1]) - static_cast<double>(box.mn[1]);
			const double dz = static_cast<double>(box.mx[2]) - static_cast<double>(box.mn[2]);
			return dx * dy + dy * dz + dz * dx;
		}

		struct FinalGroup
		{
			uint32_t level = 0;
			IAabb box{};
			int64_t cell[3] = {};
			std::vector<uint32_t> members;
		};

		struct BvhNode
		{
			IAabb box{};
			uint32_t firstChild = 0;
			uint8_t childCount = 0;
		};

		struct ClosedLeaf
		{
			uint32_t inst = 0;
			uint32_t level = 0;
			int64_t cell[3] = {};
		};

		struct BuildCtx
		{
			BuildParams params;
			uint32_t levelId = 0;
			std::vector<InstRec> inst;
			std::vector<uint32_t> levelExp;
			std::vector<LevelStats> levelStats;
			int64_t worldMin[3] = {};
			int64_t worldMax[3] = {};
			int64_t origin[3] = {};
			std::vector<FinalGroup> groups;
			std::vector<ClosedLeaf> leaves;
			uint32_t chunkSlotCount = 0;
			std::vector<BvhNode> nodes;
			std::vector<uint32_t> childRefs;
			uint32_t rootRef = 0;
			std::vector<uint32_t> slotOf;
			std::vector<uint32_t> slotInst;
			std::vector<uint32_t> groupInstBegin;
			std::vector<Node> wideNodes;
			std::vector<FloatBox> instFloat;
			std::vector<FloatBox> groupFloat;
			std::vector<FloatBox> nodeFloat;
			std::string error;
		};

		MortonKey mortonKey(const BuildCtx& ctx, uint32_t level, const IAabb& b)
		{
			SGH_VERIFY(level < ctx.levelExp.size());
			const uint32_t expL = ctx.levelExp[level < ctx.levelExp.size() ? level : 0];
			const uint32_t shift = expL >= 2 ? expL - 2 : 0u;
			uint64_t q[3] = {};
			for (int a = 0; a < 3; ++a)
			{
				const int64_t d = static_cast<int64_t>(b.mn[a]) + b.mx[a] - 2 * ctx.worldMin[a];
				SGH_VERIFY(d >= 0);
				q[a] = static_cast<uint64_t>(d < 0 ? 0 : d) >> shift;
				SGH_VERIFY(q[a] < (1ull << 32));
			}
			const uint32_t qx = static_cast<uint32_t>(q[0]);
			const uint32_t qy = static_cast<uint32_t>(q[1]);
			const uint32_t qz = static_cast<uint32_t>(q[2]);
			return interleave96(qx, qy, qz);
		}

		uint64_t mortonOfDoubled(const BuildCtx& ctx, uint32_t exp, const int64_t cell[3], const int64_t d2[3])
		{
			uint64_t s[3] = {};
			for (int a = 0; a < 3; ++a)
			{
				const int64_t c = static_cast<int64_t>(1) << exp;
				const int64_t dom2 = 2 * (ctx.origin[a] + (cell[a] << exp)) - c;
				const int64_t rel = d2[a] - dom2;
				SGH_VERIFY(rel >= 0 && rel <= (static_cast<int64_t>(1) << (exp + 2)));
				const uint32_t drop = (exp + 3 > 21) ? exp + 3 - 21 : 0;
				s[a] = static_cast<uint64_t>(rel >> drop);
			}
			return morton3(s[0], s[1], s[2]);
		}

		uint64_t localKey(const BuildCtx& ctx, uint32_t exp, const int64_t cell[3], const IAabb& b)
		{
			const int64_t d2[3] = {
				static_cast<int64_t>(b.mn[0]) + b.mx[0],
				static_cast<int64_t>(b.mn[1]) + b.mx[1],
				static_cast<int64_t>(b.mn[2]) + b.mx[2]
			};
			return mortonOfDoubled(ctx, exp, cell, d2);
		}

		bool quantizeInstance(const InstanceIn& in, uint32_t quantShift, InstRec& rec)
		{
			const double lowBound = static_cast<double>((std::numeric_limits<int32_t>::min)());
			const double highBound = static_cast<double>((std::numeric_limits<int32_t>::max)());
			const int shift = static_cast<int>(quantShift);

			for (int a = 0; a < 3; ++a)
			{
				const double qmn = std::floor(std::ldexp(in.worldMin[a], shift));
				const double qmx = std::ceil(std::ldexp(in.worldMax[a], shift));
				if (qmn < lowBound || qmn > highBound || qmx < lowBound || qmx > highBound)
				{
					return false;
				}
				rec.box.mn[a] = static_cast<int32_t>(qmn);
				rec.box.mx[a] = static_cast<int32_t>(qmx);
			}

			rec.e = ceilLog2(longestAxis(rec.box));
			return true;
		}

		bool prepare(BuildCtx& ctx, const std::vector<InstanceIn>& instances)
		{
			const uint32_t N = static_cast<uint32_t>(instances.size());
			if (N == 0)
			{
				ctx.inst.clear();
				return true;
			}

			std::vector<uint32_t> order(N);
			std::iota(order.begin(), order.end(), 0u);
			std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b)
			{
				return instances[a].key < instances[b].key;
			});

			for (uint32_t k = 1; k < N; ++k)
			{
				if (instances[order[k - 1]].key == instances[order[k]].key)
				{
					ctx.error = std::format("duplicate instance key {}", instances[order[k]].key);
					return false;
				}
			}

			for (uint32_t k = 0; k < N; ++k)
			{
				const InstanceIn& in = instances[order[k]];
				for (int a = 0; a < 3; ++a)
				{
					if (!std::isfinite(in.worldMin[a]) || !std::isfinite(in.worldMax[a]))
					{
						ctx.error = std::format("instance key {} has non-finite bounds", in.key);
						return false;
					}
					if (in.worldMin[a] > in.worldMax[a])
					{
						ctx.error = std::format("instance key {} has min > max", in.key);
						return false;
					}
				}
			}

			ctx.inst.clear();
			ctx.inst.resize(N);
			std::vector<uint8_t> failed(N, 0);

			runParallelJob(N, ctx.params.threadCount, [&](uint32_t begin, uint32_t end)
			{
				for (uint32_t k = begin; k < end; ++k)
				{
					const InstanceIn& in = instances[order[k]];
					InstRec& rec = ctx.inst[k];
					rec.key = in.key;
					rec.cullFlags = in.cullFlags;
					for (int a = 0; a < 3; ++a)
					{
						rec.srcMin[a] = in.worldMin[a];
						rec.srcMax[a] = in.worldMax[a];
					}
					failed[k] = quantizeInstance(in, ctx.params.quantShift, rec) ? 0 : 1;
				}
			});

			for (uint32_t k = 0; k < N; ++k)
			{
				if (failed[k] != 0)
				{
					ctx.error = std::format("instance key {} out of int32 range", instances[order[k]].key);
					return false;
				}
			}

			for (int a = 0; a < 3; ++a)
			{
				ctx.worldMin[a] = ctx.inst[0].box.mn[a];
				ctx.worldMax[a] = ctx.inst[0].box.mx[a];
			}
			for (uint32_t k = 1; k < N; ++k)
			{
				for (int a = 0; a < 3; ++a)
				{
					ctx.worldMin[a] = (std::min)(ctx.worldMin[a], static_cast<int64_t>(ctx.inst[k].box.mn[a]));
					ctx.worldMax[a] = (std::max)(ctx.worldMax[a], static_cast<int64_t>(ctx.inst[k].box.mx[a]));
				}
			}

			int64_t extent = 0;
			for (int a = 0; a < 3; ++a)
			{
				extent = (std::max)(extent, ctx.worldMax[a] - ctx.worldMin[a]);
			}
			if (extent > (static_cast<int64_t>(1) << WORLD_EXTENT_LOG2))
			{
				ctx.error = "world extent exceeds 2^30 quanta";
				return false;
			}

			return true;
		}

		void deriveLevels(BuildCtx& ctx)
		{
			const uint32_t N = static_cast<uint32_t>(ctx.inst.size());

			uint32_t eMin = ctx.inst[0].e;
			uint32_t eMax = ctx.inst[0].e;
			for (uint32_t k = 1; k < N; ++k)
			{
				eMin = (std::min)(eMin, ctx.inst[k].e);
				eMax = (std::max)(eMax, ctx.inst[k].e);
			}

			ctx.levelExp.clear();
			for (uint32_t e = eMin; e <= eMax + ctx.params.maxPromoteLevels; ++e)
			{
				ctx.levelExp.push_back(e);
			}
			SGH_VERIFY(ctx.levelExp.back() == eMax + ctx.params.maxPromoteLevels);
			ctx.levelStats.assign(ctx.levelExp.size(), LevelStats{});
			for (uint32_t L = 0; L < ctx.levelExp.size(); ++L)
			{
				ctx.levelStats[L].exp = ctx.levelExp[L];
			}

			const uint32_t eTop = ctx.levelExp.back() + 1;
			for (int a = 0; a < 3; ++a)
			{
				ctx.origin[a] = floorToPow2Multiple(ctx.worldMin[a], eTop);
			}

			for (uint32_t k = 0; k < N; ++k)
			{
				InstRec& r = ctx.inst[k];
				r.level = r.e - eMin;
			}
		}

		void finalizeGroup(BuildCtx& ctx, uint32_t level, const IAabb& box,
			const int64_t cell[3], const std::vector<uint32_t>& mem)
		{
			FinalGroup fg;
			fg.level = level;
			fg.box = box;
			for (int a = 0; a < 3; ++a)
			{
				fg.cell[a] = cell[a];
			}
			fg.members = mem;
			ctx.groups.push_back(std::move(fg));
		}

		void centerCell(const BuildCtx& ctx, const IAabb& b, uint32_t exp, int64_t out[3])
		{
			for (int a = 0; a < 3; ++a)
			{
				out[a] = (((static_cast<int64_t>(b.mn[a]) + b.mx[a]) >> 1) - ctx.origin[a]) >> exp;
			}
		}

		struct MergeItem
		{
			IAabb box{};
			uint32_t count = 0;
			uint32_t ref = 0;
			uint32_t id = 0;
			MortonKey key;
			uint32_t minAxis = 0;
			uint32_t maxAxis = 0;
			uint32_t risen = 0;
			bool mergedHere = false;
		};

		struct BestCand
		{
			bool valid = false;
			uint64_t cost = 0;
			uint8_t notPartner = 0;
			MortonKey keyDist;
			uint32_t dist = 0;
			uint32_t idLo = 0;
			uint32_t idHi = 0;
			uint32_t pos = 0;
		};

		struct GroupTable
		{
			std::vector<uint32_t> slots;
			std::vector<uint32_t> freeBlocks;

			uint32_t alloc()
			{
				if (!freeBlocks.empty())
				{
					const uint32_t b = freeBlocks.back();
					freeBlocks.pop_back();
					return b;
				}
				const uint32_t b = static_cast<uint32_t>(slots.size() / GROUP_MAX);
				slots.resize(slots.size() + GROUP_MAX, 0);
				return b;
			}

			void release(uint32_t b)
			{
				freeBlocks.push_back(b);
			}
		};

		std::vector<uint32_t> membersOf(const MergeItem& it, const GroupTable& gt)
		{
			SGH_VERIFY(it.count >= 1 && it.count <= GROUP_MAX);
			if (it.count < 1 || it.count > GROUP_MAX)
			{
				return {};
			}
			if (it.count == 1)
			{
				return std::vector<uint32_t>{ it.ref };
			}
			const uint32_t* base = gt.slots.data() + static_cast<size_t>(it.ref) * GROUP_MAX;
			return std::vector<uint32_t>(base, base + it.count);
		}

		uint32_t writeBlock(GroupTable& gt, const std::vector<uint32_t>& sorted)
		{
			SGH_VERIFY(sorted.size() >= 1 && sorted.size() <= GROUP_MAX);
			const uint32_t b = gt.alloc();
			for (size_t i = 0; i < sorted.size(); ++i)
			{
				gt.slots[static_cast<size_t>(b) * GROUP_MAX + i] = sorted[i];
			}
			return b;
		}

		struct MergeLimits { int64_t sL = 0; uint32_t kNum = 0; uint32_t kDen = 0; };

		enum class PairVerdict : uint8_t
		{
			Allowed,
			CountCap,
			Extent,
			SizeRatio,
			Growth
		};

		struct DefaultMergePolicy
		{
			static PairVerdict verdict(const MergeItem& a, const MergeItem& b, const IAabb& u, const MergeLimits& lim)
			{
				if (a.count + b.count > GROUP_MAX)
				{
					return PairVerdict::CountCap;
				}
				for (int i = 0; i < 3; ++i)
				{
					if (static_cast<int64_t>(u.mx[i]) - u.mn[i] > lim.sL)
					{
						return PairVerdict::Extent;
					}
				}
				if (static_cast<uint64_t>((std::max)(a.maxAxis, b.maxAxis)) > static_cast<uint64_t>(MERGE_SIZE_RATIO) * (std::min)(a.minAxis, b.minAxis))
				{
					return PairVerdict::SizeRatio;
				}
				const uint64_t scaledUnionArea = surfaceArea(u) * lim.kDen;
				if (scaledUnionArea >= static_cast<uint64_t>(lim.kNum) * surfaceArea(a.box)
					|| scaledUnionArea >= static_cast<uint64_t>(lim.kNum) * surfaceArea(b.box))
				{
					return PairVerdict::Growth;
				}
				return PairVerdict::Allowed;
			}

			static uint64_t cost(const MergeItem&, const MergeItem&, const IAabb& u)
			{
				return surfaceArea(u);
			}

			static bool better(const BestCand& a, const BestCand& b)
			{
				if (a.cost != b.cost)
				{
					return a.cost < b.cost;
				}
				if (a.notPartner != b.notPartner)
				{
					return a.notPartner < b.notPartner;
				}
				if (a.keyDist != b.keyDist)
				{
					return a.keyDist < b.keyDist;
				}
				if (a.dist != b.dist)
				{
					return a.dist < b.dist;
				}
				if (a.idLo != b.idLo)
				{
					return a.idLo < b.idLo;
				}
				return a.idHi < b.idHi;
			}
		};

		constexpr uint32_t NO_BLOCK = (std::numeric_limits<uint32_t>::max)();

		uint32_t keyShiftFor(uint32_t expL)
		{
			return expL >= 2 ? expL - 2 : 0u;
		}

		constexpr uint32_t BLOCK_EDGE_SHIFT = 1;

		uint32_t blockBitsFor(uint32_t expL)
		{
			const uint32_t result = expL + 1 + BLOCK_EDGE_SHIFT - keyShiftFor(expL);
			SGH_VERIFY(result >= 1 && result <= 4);
			return result;
		}

		struct BlockRange
		{
			MortonKey prefix;
			uint32_t begin = 0;
			uint32_t end = 0;
			uint32_t coord[3] = {};
		};

		struct BlockTable
		{
			uint32_t bits = 0;
			uint32_t shift = 0;
			std::vector<BlockRange> blocks;
			std::vector<uint32_t> blockOf;
			std::vector<uint32_t> neighbors;
		};

		void buildBlockTable(const BuildCtx& ctx, uint32_t expL, const std::vector<MergeItem>& pool, BlockTable& out)
		{
			const uint32_t n = static_cast<uint32_t>(pool.size());
			const uint32_t bits = blockBitsFor(expL);
			const uint32_t shift = keyShiftFor(expL);
			out.bits = bits;
			out.shift = shift;
			out.blocks.clear();
			out.blockOf.assign(n, 0);

			for (uint32_t p = 0; p < n; ++p)
			{
				const MergeItem& item = pool[p];
				if (p > 0)
				{
					SGH_VERIFY(pool[p - 1].key < item.key || (pool[p - 1].key == item.key && pool[p - 1].id < item.id));
				}

				uint32_t q[3] = {};
				deinterleave96(item.key, q);
				for (int a = 0; a < 3; ++a)
				{
					const int64_t cellLo = static_cast<int64_t>(q[a]) << shift;
					const int64_t cellHi = (static_cast<int64_t>(q[a]) + 1) << shift;
					const int64_t boxLo = 2 * static_cast<int64_t>(item.box.mn[a]) - 2 * ctx.worldMin[a];
					const int64_t boxHi = 2 * static_cast<int64_t>(item.box.mx[a]) - 2 * ctx.worldMin[a];
					SGH_VERIFY(cellLo <= boxHi && cellHi > boxLo);
				}

				const MortonKey prefix = shr128(item.key, 3 * bits);
				if (out.blocks.empty() || out.blocks.back().prefix != prefix)
				{
					SGH_VERIFY(out.blocks.empty() || out.blocks.back().prefix < prefix);
					SGH_VERIFY(interleave96(q[0], q[1], q[2]) == item.key);
					BlockRange block;
					block.prefix = prefix;
					block.begin = p;
					block.end = p + 1;
					for (int a = 0; a < 3; ++a)
					{
						block.coord[a] = q[a] >> bits;
					}
					out.blocks.push_back(block);
				}
				else
				{
					out.blocks.back().end = p + 1;
				}
				out.blockOf[p] = static_cast<uint32_t>(out.blocks.size() - 1);
			}

			if (n > 0)
			{
				SGH_VERIFY(out.blocks.front().begin == 0);
				SGH_VERIFY(out.blocks.back().end == n);
			}
			for (size_t b = 0; b < out.blocks.size(); ++b)
			{
				SGH_VERIFY(out.blocks[b].begin < out.blocks[b].end);
				if (b > 0)
				{
					SGH_VERIFY(out.blocks[b - 1].end == out.blocks[b].begin);
				}
			}
		}

		uint32_t findBlockIndex(const std::vector<BlockRange>& blocks, uint32_t from, const MortonKey& target)
		{
			const uint64_t count = blocks.size();
			SGH_VERIFY(from < count);
			if (blocks[from].prefix == target)
			{
				return from;
			}

			uint64_t lo = 0;
			uint64_t hi = from;
			uint64_t step = 1;
			if (blocks[from].prefix < target)
			{
				lo = static_cast<uint64_t>(from) + 1;
				while (from + step < count && blocks[from + step].prefix < target)
				{
					lo = from + step + 1;
					step *= 2;
				}
				hi = (std::min)(count, static_cast<uint64_t>(from) + step + 1);
			}
			else
			{
				while (step <= from && !(blocks[from - step].prefix < target))
				{
					hi = from - step + 1;
					step *= 2;
				}
				lo = (step <= from) ? from - step + 1 : 0;
			}

			const auto first = blocks.begin() + static_cast<std::ptrdiff_t>(lo);
			const auto last = blocks.begin() + static_cast<std::ptrdiff_t>(hi);
			const auto it = std::lower_bound(first, last, target, [](const BlockRange& b, const MortonKey& k)
			{
				return b.prefix < k;
			});
			if (it != blocks.end() && it->prefix == target)
			{
				return static_cast<uint32_t>(it - blocks.begin());
			}
			return NO_BLOCK;
		}

		void resolveNeighbors(const BuildCtx& ctx, BlockTable& t)
		{
			const uint32_t blockCount = static_cast<uint32_t>(t.blocks.size());
			t.neighbors.assign(static_cast<size_t>(27) * blockCount, NO_BLOCK);
			const int64_t coordLimit = static_cast<int64_t>(1) << 32;

			runParallelJob(blockCount, ctx.params.threadCount, [&](uint32_t begin, uint32_t end)
			{
				for (uint32_t b = begin; b < end; ++b)
				{
					const BlockRange& block = t.blocks[b];
					for (int o = 0; o < 27; ++o)
					{
						const int64_t cx = static_cast<int64_t>(block.coord[0]) + (o % 3) - 1;
						const int64_t cy = static_cast<int64_t>(block.coord[1]) + ((o / 3) % 3) - 1;
						const int64_t cz = static_cast<int64_t>(block.coord[2]) + (o / 9) - 1;
						if (cx < 0 || cy < 0 || cz < 0)
						{
							continue;
						}
						if ((cx << t.bits) >= coordLimit || (cy << t.bits) >= coordLimit || (cz << t.bits) >= coordLimit)
						{
							continue;
						}
						const MortonKey target = interleave96(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), static_cast<uint32_t>(cz));
						t.neighbors[static_cast<size_t>(27) * b + static_cast<size_t>(o)] = findBlockIndex(t.blocks, b, target);
					}
				}
			});
		}

		void remapNeighbors(const BlockTable& prev, BlockTable& next)
		{
			const size_t prevCount = prev.blocks.size();
			const size_t nextCount = next.blocks.size();
			SGH_VERIFY(prev.neighbors.size() == static_cast<size_t>(27) * prevCount);
			std::vector<uint32_t> newOf(prevCount, NO_BLOCK);
			std::vector<uint32_t> oldOf(nextCount, NO_BLOCK);
			size_t nb = 0;
			for (size_t ob = 0; ob < prevCount && nb < nextCount; ++ob)
			{
				if (prev.blocks[ob].prefix == next.blocks[nb].prefix)
				{
					newOf[ob] = static_cast<uint32_t>(nb);
					oldOf[nb] = static_cast<uint32_t>(ob);
					nb += 1;
				}
			}
			SGH_VERIFY(nb == nextCount);

			next.neighbors.assign(static_cast<size_t>(27) * nextCount, NO_BLOCK);
			for (size_t b = 0; b < nextCount; ++b)
			{
				if (oldOf[b] == NO_BLOCK)
				{
					continue;
				}
				for (size_t o = 0; o < 27; ++o)
				{
					const uint32_t oldTarget = prev.neighbors[static_cast<size_t>(27) * oldOf[b] + o];
					const uint32_t mapped = (oldTarget == NO_BLOCK) ? NO_BLOCK : newOf[oldTarget];
					SGH_VERIFY(mapped == NO_BLOCK || mapped < nextCount);
					next.neighbors[27 * b + o] = mapped;
				}
			}
		}

		bool insideSearchBox(const IAabb& cand, const IAabb& self, int64_t sL)
		{
			for (int a = 0; a < 3; ++a)
			{
				if (static_cast<int64_t>(cand.mn[a]) < static_cast<int64_t>(self.mx[a]) - sL
					|| static_cast<int64_t>(cand.mx[a]) > static_cast<int64_t>(self.mn[a]) + sL)
				{
					return false;
				}
			}
			return true;
		}

		BestCand makeCandidate(const std::vector<MergeItem>& pool, uint32_t p, uint32_t j, uint64_t cost)
		{
			BestCand c;
			c.valid = true;
			c.cost = cost;
			c.notPartner = (j == (p ^ 1u)) ? 0 : 1;
			c.keyDist = absDiff128(pool[p].key, pool[j].key);
			c.dist = j > p ? j - p : p - j;
			c.idLo = (std::min)(pool[p].id, pool[j].id);
			c.idHi = (std::max)(pool[p].id, pool[j].id);
			c.pos = j;
			return c;
		}

		uint32_t growthBucket(uint64_t saUnion, uint64_t saMin)
		{
			static_assert(GROWTH_BUCKETS == 6);
			SGH_VERIFY(saMin >= 1 && saUnion >= saMin);
			if (saMin < 1 || saUnion < saMin)
			{
				return 0;
			}
			if (2 * saUnion < 3 * saMin)
			{
				return 0;
			}
			if (saUnion < 2 * saMin)
			{
				return 1;
			}
			if (2 * saUnion < 5 * saMin)
			{
				return 2;
			}
			if (saUnion < 3 * saMin)
			{
				return 3;
			}
			if (saUnion < 4 * saMin)
			{
				return 4;
			}
			return 5;
		}

		struct PairEval
		{
			PairVerdict verdict = PairVerdict::Allowed;
			uint32_t growthBucket = 0;
		};

		template <typename Policy>
		PairEval evalCandidate(const std::vector<MergeItem>& pool, uint32_t p, uint32_t j, const MergeLimits& lim, BestCand& out)
		{
			const MergeItem& a = pool[p];
			const MergeItem& b = pool[j];
			const IAabb u = unionBox(a.box, b.box);
			PairEval eval;
			eval.verdict = Policy::verdict(a, b, u, lim);
			eval.growthBucket = growthBucket(surfaceArea(u), (std::min)(surfaceArea(a.box), surfaceArea(b.box)));
			if (eval.verdict != PairVerdict::Allowed)
			{
				return eval;
			}
			const BestCand cand = makeCandidate(pool, p, j, Policy::cost(a, b, u));
			if (!out.valid || Policy::better(cand, out))
			{
				out = cand;
			}
			return eval;
		}

		struct SearchCounters
		{
			uint64_t aabbTests = 0;
			uint64_t aabbPasses = 0;
			uint64_t pairEvals = 0;
			uint64_t rejectCount = 0;
			uint64_t rejectExtent = 0;
			uint64_t rejectRatio = 0;
			uint64_t rejectGrowth = 0;
			uint64_t growthHist[GROWTH_BUCKETS] = {};
			uint64_t growthHistKTested[GROWTH_BUCKETS] = {};
		};

		template <typename Policy>
		void findBest(const BuildCtx& ctx, const std::vector<MergeItem>& pool, const BlockTable& t, const MergeLimits& lim,
			std::vector<BestCand>& best, std::vector<uint32_t>* passCount, RoundStats& rs)
		{
			const uint32_t n = static_cast<uint32_t>(pool.size());
			SGH_VERIFY(best.size() == n && t.blockOf.size() == n);
			SGH_VERIFY(passCount == nullptr || passCount->size() == n);
			std::vector<SearchCounters> counters((static_cast<size_t>(n) + PAR_CHUNK - 1) / PAR_CHUNK);

			runParallelJob(n, ctx.params.threadCount, [&](uint32_t begin, uint32_t end)
			{
				SearchCounters& cnt = counters[begin / PAR_CHUNK];
				for (uint32_t p = begin; p < end; ++p)
				{
					const MergeItem& self = pool[p];
					const uint32_t ownIdx = t.blockOf[p];
					const BlockRange& own = t.blocks[ownIdx];
					int32_t offset[3] = {};
					for (int a = 0; a < 3; ++a)
					{
						const int64_t loD = (std::max)(int64_t{ 0 }, 2 * (static_cast<int64_t>(self.box.mx[a]) - lim.sL) - 2 * ctx.worldMin[a]);
						const int64_t hiD = 2 * (static_cast<int64_t>(self.box.mn[a]) + lim.sL) - 2 * ctx.worldMin[a];
						const int64_t ownCoord = static_cast<int64_t>(own.coord[a]);
						SGH_VERIFY(loD >= 0 && hiD >= loD);
						const int64_t lo = static_cast<int64_t>((static_cast<uint64_t>(loD) >> t.shift) >> t.bits);
						const int64_t hi = static_cast<int64_t>((static_cast<uint64_t>(hiD) >> t.shift) >> t.bits);
						SGH_VERIFY(lo >= ownCoord - 1 && hi <= ownCoord + 1 && hi - lo <= 1 && lo <= ownCoord && ownCoord <= hi);
						offset[a] = (lo < ownCoord) ? -1 : ((hi > ownCoord) ? 1 : 0);
					}

					BestCand out;
					for (uint32_t mask = 0; mask < 8; ++mask)
					{
						int32_t step[3] = {};
						bool reachable = true;
						for (int a = 0; a < 3; ++a)
						{
							const bool takesStep = ((mask >> a) & 1u) != 0;
							if (takesStep && offset[a] == 0)
							{
								reachable = false;
							}
							step[a] = takesStep ? offset[a] : 0;
						}
						if (!reachable)
						{
							continue;
						}
						const int32_t neighborSlot = 13 + step[2] * 9 + step[1] * 3 + step[0];
						const uint32_t nb = t.neighbors[static_cast<size_t>(27) * ownIdx + static_cast<size_t>(neighborSlot)];
						if (nb == NO_BLOCK)
						{
							continue;
						}
						const BlockRange& block = t.blocks[nb];
						for (uint32_t j = block.begin; j < block.end; ++j)
						{
							if (j == p)
							{
								continue;
							}
							cnt.aabbTests += 1;
							if (!insideSearchBox(pool[j].box, self.box, lim.sL))
							{
								continue;
							}
							cnt.aabbPasses += 1;
							const PairEval eval = evalCandidate<Policy>(pool, p, j, lim, out);
							cnt.growthHist[eval.growthBucket] += 1;
							switch (eval.verdict)
							{
							case PairVerdict::CountCap:
								cnt.rejectCount += 1;
								break;
							case PairVerdict::Extent:
								cnt.rejectExtent += 1;
								break;
							case PairVerdict::SizeRatio:
								cnt.rejectRatio += 1;
								break;
							case PairVerdict::Growth:
								cnt.rejectGrowth += 1;
								cnt.growthHistKTested[eval.growthBucket] += 1;
								break;
							case PairVerdict::Allowed:
								cnt.growthHistKTested[eval.growthBucket] += 1;
								break;
							}
							if (eval.verdict != PairVerdict::Allowed)
							{
								continue;
							}
							cnt.pairEvals += 1;
							if (passCount != nullptr)
							{
								(*passCount)[p] += 1;
							}
						}
					}
					best[p] = out;
				}
			});

			uint64_t rejectExtent = 0;
			for (const SearchCounters& cnt : counters)
			{
				rs.aabbTests += cnt.aabbTests;
				rs.aabbPasses += cnt.aabbPasses;
				rs.pairEvals += cnt.pairEvals;
				rs.rejectCount += cnt.rejectCount;
				rejectExtent += cnt.rejectExtent;
				rs.rejectRatio += cnt.rejectRatio;
				rs.rejectGrowth += cnt.rejectGrowth;
				for (uint32_t bucket = 0; bucket < GROWTH_BUCKETS; ++bucket)
				{
					rs.growthHist[bucket] += cnt.growthHist[bucket];
					rs.growthHistKTested[bucket] += cnt.growthHistKTested[bucket];
				}
			}

			SGH_VERIFY(rejectExtent == 0);
			SGH_VERIFY(rs.aabbPasses == rs.rejectCount + rs.rejectRatio + rs.rejectGrowth + rs.pairEvals);
			SGH_VERIFY(rs.aabbPasses % 2 == 0 && rs.pairEvals % 2 == 0);
			SGH_VERIFY(rs.rejectCount % 2 == 0 && rs.rejectRatio % 2 == 0 && rs.rejectGrowth % 2 == 0);
			uint64_t histTotal = 0;
			uint64_t kTestedTotal = 0;
			for (uint32_t bucket = 0; bucket < GROWTH_BUCKETS; ++bucket)
			{
				SGH_VERIFY(rs.growthHist[bucket] % 2 == 0 && rs.growthHistKTested[bucket] % 2 == 0);
				histTotal += rs.growthHist[bucket];
				kTestedTotal += rs.growthHistKTested[bucket];
			}
			SGH_VERIFY(histTotal == rs.aabbPasses);
			SGH_VERIFY(kTestedTotal == rs.rejectGrowth + rs.pairEvals);
		}

		template <typename Policy>
		void findBestBrute(const BuildCtx& ctx, const std::vector<MergeItem>& pool, const MergeLimits& lim,
			std::vector<BestCand>& best, std::vector<uint32_t>& passCount)
		{
			const uint32_t n = static_cast<uint32_t>(pool.size());
			SGH_VERIFY(best.size() == n && passCount.size() == n);

			runParallelJob(n, ctx.params.threadCount, [&](uint32_t begin, uint32_t end)
			{
				for (uint32_t p = begin; p < end; ++p)
				{
					BestCand out;
					for (uint32_t j = 0; j < n; ++j)
					{
						if (j == p)
						{
							continue;
						}
						if (evalCandidate<Policy>(pool, p, j, lim, out).verdict == PairVerdict::Allowed)
						{
							passCount[p] += 1;
						}
					}
					best[p] = out;
				}
			});
		}

		uint32_t applyMerges(BuildCtx& ctx, uint32_t L, std::vector<MergeItem>& pool, GroupTable& gt,
			const std::vector<BestCand>& best, std::vector<uint8_t>& dead, uint32_t& mergedLive, uint32_t& unmergedLive, RoundStats& rs)
		{
			const uint32_t n = static_cast<uint32_t>(pool.size());
			SGH_VERIFY(best.size() == n && dead.size() == n);
			const uint32_t cexp = ctx.levelExp[L] + 1;
			uint32_t merges = 0;
			uint32_t finalized = 0;

			for (uint32_t i = 0; i < n; ++i)
			{
				if (!best[i].valid)
				{
					continue;
				}
				const uint32_t j = best[i].pos;
				if (i >= j || !best[j].valid || best[j].pos != i)
				{
					continue;
				}

				SGH_VERIFY(dead[i] == 0 && dead[j] == 0);
				if (dead[i] != 0 || dead[j] != 0)
				{
					continue;
				}

				const std::vector<uint32_t> ma = membersOf(pool[i], gt);
				const std::vector<uint32_t> mb = membersOf(pool[j], gt);
				std::vector<uint32_t> merged(ma.size() + mb.size());
				std::merge(ma.begin(), ma.end(), mb.begin(), mb.end(), merged.begin());
				const IAabb u = unionBox(pool[i].box, pool[j].box);
				const uint32_t count = pool[i].count + pool[j].count;
				const uint32_t minAxis = (std::min)(pool[i].minAxis, pool[j].minAxis);
				const uint32_t maxAxis = (std::max)(pool[i].maxAxis, pool[j].maxAxis);
				const uint32_t id = pool[i].id;
				const MortonKey key = pool[i].key;

				for (const uint32_t side : { i, j })
				{
					if (pool[side].count >= 2)
					{
						gt.release(pool[side].ref);
						SGH_VERIFY(mergedLive >= 1);
						mergedLive -= 1;
					}
					else
					{
						SGH_VERIFY(unmergedLive >= 1);
						unmergedLive -= 1;
					}
				}

				if (merged.size() == GROUP_MAX)
				{
					int64_t cell[3] = {};
					centerCell(ctx, u, cexp, cell);
					finalizeGroup(ctx, L, u, cell, merged);
					dead[i] = 1;
					finalized += 1;
				}
				else
				{
					MergeItem g;
					g.box = u;
					g.count = count;
					g.ref = writeBlock(gt, merged);
					g.id = id;
					g.key = key;
					g.minAxis = minAxis;
					g.maxAxis = maxAxis;
					g.mergedHere = true;
					pool[i] = g;
					mergedLive += 1;
				}
				dead[j] = 1;
				merges += 1;
			}

			rs.merges = merges;
			rs.finalized = finalized;
			return merges;
		}

		template <typename Policy>
		void runRounds(BuildCtx& ctx, uint32_t L, std::vector<MergeItem>& pool, GroupTable& gt)
		{
			const uint32_t expL = ctx.levelExp[L];
			const MergeLimits lim{ static_cast<int64_t>(1) << expL, ctx.params.mergeKNum, ctx.params.mergeKDen };
			LevelStats& stats = ctx.levelStats[L];
			BlockTable table;
			BlockTable previous;
			uint32_t mergedLive = 0;
			uint32_t unmergedLive = 0;
			for (const MergeItem& poolItem : pool)
			{
				if (poolItem.count >= 2)
				{
					mergedLive += 1;
				}
				else
				{
					unmergedLive += 1;
				}
			}
			SGH_VERIFY(mergedLive + unmergedLive == pool.size());

			const uint32_t initialSize = static_cast<uint32_t>(pool.size());
			for (uint32_t round = 0; round < initialSize && pool.size() >= 2; ++round)
			{
				const uint32_t n = static_cast<uint32_t>(pool.size());

				stats.roundStats.emplace_back();
				RoundStats& rs = stats.roundStats.back();
				rs.mergedPool = mergedLive;
				rs.unmergedPool = unmergedLive;

				std::swap(previous, table);
				buildBlockTable(ctx, expL, pool, table);
				if (round == 0)
				{
					resolveNeighbors(ctx, table);
				}
				else
				{
					remapNeighbors(previous, table);
				}
				previous = BlockTable{};
				rs.blockCount = static_cast<uint32_t>(table.blocks.size());
				for (const BlockRange& block : table.blocks)
				{
					rs.maxBlockPop = (std::max)(rs.maxBlockPop, block.end - block.begin);
				}

				std::vector<BestCand> best(n);
				std::vector<uint32_t> bestPass;
				if (ctx.params.crossCheckBrute)
				{
					bestPass.assign(n, 0);
				}
				findBest<Policy>(ctx, pool, table, lim, best, ctx.params.crossCheckBrute ? &bestPass : nullptr, rs);

				if (ctx.params.crossCheckBrute)
				{
					std::vector<BestCand> bruteBest(n);
					std::vector<uint32_t> brutePass(n, 0);
					findBestBrute<Policy>(ctx, pool, lim, bruteBest, brutePass);
					for (uint32_t p = 0; p < n; ++p)
					{
						SGH_VERIFY(bestPass[p] == brutePass[p]);
						SGH_VERIFY(best[p].valid == bruteBest[p].valid);
						SGH_VERIFY(!best[p].valid || best[p].pos == bruteBest[p].pos);
					}
				}

				const bool anyValid = std::any_of(best.begin(), best.end(), [](const BestCand& c)
				{
					return c.valid;
				});
				if (!anyValid)
				{
					break;
				}

				std::vector<uint8_t> dead(n, 0);
				const uint32_t merged = applyMerges(ctx, L, pool, gt, best, dead, mergedLive, unmergedLive, rs);
				SGH_VERIFY(merged >= 1);

				std::vector<MergeItem> next;
				next.reserve(n);
				for (uint32_t i = 0; i < n; ++i)
				{
					if (dead[i] == 0)
					{
						next.push_back(pool[i]);
					}
				}
				pool.swap(next);
				SGH_VERIFY(mergedLive + unmergedLive == pool.size());
			}

			if (ctx.params.crossCheckBrute && pool.size() >= 2)
			{
				const uint32_t n = static_cast<uint32_t>(pool.size());
				std::vector<BestCand> bruteBest(n);
				std::vector<uint32_t> brutePass(n, 0);
				findBestBrute<Policy>(ctx, pool, lim, bruteBest, brutePass);
				for (uint32_t p = 0; p < n; ++p)
				{
					SGH_VERIFY(!bruteBest[p].valid);
				}
			}
		}

		template <typename Policy>
		void mergeLevel(BuildCtx& ctx, uint32_t L, std::vector<std::vector<MergeItem>>& pools, GroupTable& gt)
		{
			const uint32_t expL = ctx.levelExp[L];
			const uint32_t cexp = expL + 1;
			const int64_t sL = static_cast<int64_t>(1) << expL;
			const uint32_t topLevel = static_cast<uint32_t>(ctx.levelExp.size()) - 1;
			std::vector<MergeItem>& pool = pools[L];
			LevelStats& stats = ctx.levelStats[L];
			const auto mergeStart = std::chrono::steady_clock::now();

			std::sort(pool.begin(), pool.end(), [](const MergeItem& a, const MergeItem& b)
			{
				if (a.key != b.key)
				{
					return a.key < b.key;
				}
				return a.id < b.id;
			});

			const uint32_t instanceCount = static_cast<uint32_t>(ctx.inst.size());
			std::vector<uint8_t> seenId(instanceCount, 0);
			for (const MergeItem& it : pool)
			{
				SGH_VERIFY(it.id < instanceCount && seenId[it.id] == 0);
				if (it.id >= instanceCount)
				{
					continue;
				}
				seenId[it.id] = 1;
			}

			runRounds<Policy>(ctx, L, pool, gt);

			for (MergeItem& it : pool)
			{
				SGH_VERIFY(it.count >= 2 || !it.mergedHere);
				SGH_VERIFY(it.count >= 2 || it.risen <= ctx.params.maxPromoteLevels);
				if (it.count == 1)
				{
					SGH_VERIFY(it.ref < ctx.inst.size());
					if (it.ref >= ctx.inst.size())
					{
						continue;
					}
					SGH_VERIFY(L == ctx.inst[it.ref].level + it.risen);
				}
				const int64_t longestLength = longestAxis(it.box);
				SGH_VERIFY(longestLength <= sL);

				const bool moveUp = L < topLevel && (it.count >= 2 ? it.mergedHere : it.risen < ctx.params.maxPromoteLevels);
				if (moveUp)
				{
					it.mergedHere = false;
					if (it.count == 1)
					{
						it.risen += 1;
					}
					it.key = mortonKey(ctx, L + 1, it.box);
					pools[L + 1].push_back(it);
					continue;
				}

				const std::vector<uint32_t> mem = membersOf(it, gt);
				if (it.count >= 2)
				{
					gt.release(it.ref);
				}
				int64_t cell[3] = {};
				centerCell(ctx, it.box, cexp, cell);
				if (it.count >= 2)
				{
					finalizeGroup(ctx, L, it.box, cell, mem);
				}
				else
				{
					ClosedLeaf leaf;
					leaf.inst = mem[0];
					leaf.level = L;
					for (int a = 0; a < 3; ++a)
					{
						leaf.cell[a] = cell[a];
					}
					ctx.leaves.push_back(leaf);
				}
			}

			stats.mergeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - mergeStart).count();
			pool.clear();
			pool.shrink_to_fit();
		}

		void sweepLevels(BuildCtx& ctx)
		{
			const uint32_t N = static_cast<uint32_t>(ctx.inst.size());
			const uint32_t levelCount = static_cast<uint32_t>(ctx.levelExp.size());

			std::vector<std::vector<MergeItem>> pools(levelCount);
			for (uint32_t k = 0; k < N; ++k)
			{
				const InstRec& r = ctx.inst[k];
				MergeItem it;
				it.box = r.box;
				it.count = 1;
				it.ref = k;
				it.id = k;
				it.key = mortonKey(ctx, r.level, r.box);
				it.minAxis = static_cast<uint32_t>(longestAxis(r.box));
				it.maxAxis = it.minAxis;
				pools[r.level].push_back(it);
			}

			GroupTable gt;
			for (uint32_t L = 0; L < levelCount; ++L)
			{
				if (pools[L].empty())
				{
					continue;
				}
				mergeLevel<DefaultMergePolicy>(ctx, L, pools, gt);
			}

			std::vector<uint8_t> seen(N, 0);
			uint32_t total = 0;
			for (const FinalGroup& fg : ctx.groups)
			{
				for (const uint32_t k : fg.members)
				{
					SGH_VERIFY(seen[k] == 0);
					seen[k] = 1;
					total += 1;
				}
			}
			for (const ClosedLeaf& leaf : ctx.leaves)
			{
				SGH_VERIFY(seen[leaf.inst] == 0);
				seen[leaf.inst] = 1;
				total += 1;
			}
			SGH_VERIFY(total == N);

			for (const ClosedLeaf& leaf : ctx.leaves)
			{
				SGH_VERIFY(leaf.inst < N);
				if (leaf.inst >= N)
				{
					continue;
				}
				SGH_VERIFY(leaf.level == ctx.inst[leaf.inst].level + ctx.params.maxPromoteLevels);
			}

			for (const FinalGroup& fg : ctx.groups)
			{
				SGH_VERIFY(fg.level < ctx.levelStats.size());
				SGH_VERIFY(fg.members.size() <= GROUP_MAX);
				if (fg.level >= ctx.levelStats.size() || fg.members.size() > GROUP_MAX)
				{
					continue;
				}
				LevelStats& stats = ctx.levelStats[fg.level];
				stats.groupCount += 1;
				stats.fill[fg.members.size()] += 1;
				stats.areaSum += static_cast<double>(surfaceArea(fg.box));
				stats.groupAreaSum += static_cast<double>(surfaceArea(fg.box));
			}
			for (const ClosedLeaf& leaf : ctx.leaves)
			{
				SGH_VERIFY(leaf.level < ctx.levelStats.size());
				if (leaf.level >= ctx.levelStats.size())
				{
					continue;
				}
				LevelStats& stats = ctx.levelStats[leaf.level];
				stats.leafCount += 1;
				stats.areaSum += static_cast<double>(surfaceArea(ctx.inst[leaf.inst].box));
			}
			for (const LevelStats& ls : ctx.levelStats)
			{
				SGH_VERIFY(ls.groupAreaSum <= ls.areaSum);
				SGH_VERIFY((ls.groupCount == 0) == (ls.groupAreaSum == 0.0));
			}
		}
		void buildBvh(BuildCtx& ctx)
		{
		}
		void renumberDfs(BuildCtx& ctx)
		{
		}
		void assertInvariants(const BuildCtx& ctx)
		{
		}
		void polishBounds(BuildCtx& ctx)
		{
		}
		void buildWideNodes(BuildCtx& ctx)
		{
		}
	void setAssertHandler(AssertHandler handler)
	{
		g_assertHandler = (handler != nullptr) ? handler : &defaultAssertHandler;
	}

	uint64_t hashBytes(const std::vector<uint8_t>& bytes)
	{
		return fnv1a64(bytes.data(), bytes.size());
	}

	ReadResult readFile(const std::vector<uint8_t>& bytes)
	{
		ReadResult result;
		result.error = readHeader(bytes, result.file);
		if (result.error.empty())
		{
			result.error = readSections(bytes, result.file);
		}
		if (result.error.empty())
		{
			result.error = checkStructure(result.file);
		}
		result.ok = result.error.empty();
		return result;
	}
	BuildResult build(const std::vector<InstanceIn>& instances, uint32_t levelId, const BuildParams& params)
	{
		BuildResult result;

		if (params.bvhWidth < 2 || params.bvhWidth > NODE_WIDTH)
		{
			result.error = "bvhWidth must be in [2, 8]";
			return result;
		}
		if (params.quantShift > 16)
		{
			result.error = "quantShift must be <= 16";
			return result;
		}
		if (params.mergeKDen < 1 || params.mergeKNum <= params.mergeKDen || params.mergeKNum > MERGE_K_MAX)
		{
			result.error = std::format("merge growth k must satisfy 1 <= mergeKDen < mergeKNum <= {}", MERGE_K_MAX);
			return result;
		}
		if (params.maxPromoteLevels > MAX_PROMOTE_LEVELS)
		{
			result.error = std::format("maxPromoteLevels must be <= {}", MAX_PROMOTE_LEVELS);
			return result;
		}
		if (instances.size() > CHILD_INST_BIT)
		{
			result.error = "instance count exceeds 2^30";
			return result;
		}

		const auto totalStart = std::chrono::steady_clock::now();
		BuildCtx ctx;
		ctx.params = params;
		ctx.levelId = levelId;

		if (!prepare(ctx, instances))
		{
			result.error = ctx.error;
			return result;
		}

		auto sweepStart = std::chrono::steady_clock::now();
		auto bvhStart = sweepStart;
		if (ctx.inst.empty())
		{
			ctx.rootRef = CHILD_EMPTY;
		}
		else
		{
			deriveLevels(ctx);
			sweepStart = std::chrono::steady_clock::now();
			sweepLevels(ctx);
			bvhStart = std::chrono::steady_clock::now();
			buildBvh(ctx);
			renumberDfs(ctx);
			assertInvariants(ctx);
			polishBounds(ctx);
			buildWideNodes(ctx);
			assertWideNodes(ctx);
		}

		serialize(ctx, result.bytes);
		result.contentHash = hashBytes(result.bytes);
		const auto bvhEnd = std::chrono::steady_clock::now();

		const ReadResult check = readFile(result.bytes);
		SGH_VERIFY(check.ok);
		SGH_VERIFY(check.file.nodeCount == ctx.nodes.size()
			&& check.file.instanceCount == ctx.inst.size()
			&& check.file.groupCount == ctx.groups.size());

		result.stats.levelCount = static_cast<uint32_t>(ctx.levelExp.size());
		result.stats.instanceCount = static_cast<uint32_t>(ctx.inst.size());
		result.stats.groupCount = static_cast<uint32_t>(ctx.groups.size());
		result.stats.nodeCount = static_cast<uint32_t>(ctx.nodes.size());
		result.stats.levels = ctx.levelStats;
		result.stats.threadCount = params.threadCount;
		result.stats.sweepMs = std::chrono::duration<double, std::milli>(bvhStart - sweepStart).count();
		result.stats.bvhMs = std::chrono::duration<double, std::milli>(bvhEnd - bvhStart).count();
		result.stats.totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - totalStart).count();
		measureBoundsShrink(ctx, result.stats);
		result.ok = true;
		return result;
	}
}
