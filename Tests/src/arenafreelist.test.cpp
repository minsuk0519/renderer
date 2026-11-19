#include "property_harness.hpp"

#include <render/buffer.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace
{
    struct freeBlock
    {
        uint start;
        uint size;
    };

    std::vector<freeBlock> snapshot(const arena_free_list& list)
    {
        std::vector<freeBlock> blocks;
        for (uint i = 0; i < list.blockCount(); ++i)
        {
            freeBlock b{};
            const bool ok = list.block(i, b.start, b.size);
            if (!ok)
            {
                return {};
            }
            blocks.push_back(b);
        }
        return blocks;
    }

    bool sameBlocks(const std::vector<freeBlock>& a, const std::vector<freeBlock>& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].start != b[i].start || a[i].size != b[i].size)
            {
                return false;
            }
        }
        return true;
    }

    bool structurallySound(const arena_free_list& list)
    {
        const std::vector<freeBlock> blocks = snapshot(list);
        if (blocks.size() != list.blockCount())
        {
            return false;
        }

        std::uint64_t freeTotal = 0;
        std::uint64_t previousEnd = 0;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            const freeBlock& b = blocks[i];
            if (b.size == 0)
            {
                return false;
            }
            if (i > 0 && static_cast<std::uint64_t>(b.start) <= previousEnd)
            {
                return false;
            }
            const std::uint64_t end = static_cast<std::uint64_t>(b.start) + b.size;
            if (end > list.capacity())
            {
                return false;
            }
            previousEnd = end;
            freeTotal += b.size;
        }

        return freeTotal + list.usedBytes() == list.capacity();
    }

    struct liveBlock
    {
        uint start;
        uint size;
    };

    bool bytesOverlap(const liveBlock& a, const liveBlock& b)
    {
        return static_cast<std::uint64_t>(a.start) < static_cast<std::uint64_t>(b.start) + b.size &&
               static_cast<std::uint64_t>(b.start) < static_cast<std::uint64_t>(a.start) + a.size;
    }
}

TEST(arenaFreeList_p6_reset_zero_capacity_is_empty_and_rejects_everything)
{
    arena_free_list list;
    list.reset(0);

    uint start = 77;
    EXPECT_EQ(list.capacity(), 0u);
    EXPECT_EQ(list.blockCount(), 0u);
    EXPECT_EQ(list.usedBytes(), 0u);
    EXPECT_FALSE(list.reserve(1, start));
    EXPECT_FALSE(list.reserve(0, start));
    EXPECT_FALSE(list.release(0, 1));
    EXPECT_FALSE(list.release(0, 0));
    EXPECT_TRUE(structurallySound(list));
}

TEST(arenaFreeList_p6_default_constructed_list_rejects_reserve_and_release)
{
    arena_free_list list;
    uint start = 0;
    EXPECT_EQ(list.capacity(), 0u);
    EXPECT_FALSE(list.reserve(1, start));
    EXPECT_FALSE(list.release(0, 1));
    EXPECT_EQ(list.blockCount(), 0u);
}

TEST(arenaFreeList_p6_reserve_zero_size_fails_and_leaves_state_unchanged)
{
    arena_free_list list;
    list.reset(128);
    const std::vector<freeBlock> before = snapshot(list);

    uint start = 999;
    EXPECT_FALSE(list.reserve(0, start));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
    EXPECT_EQ(list.usedBytes(), 0u);
}

TEST(arenaFreeList_p6_reserve_larger_than_capacity_fails_and_leaves_state_unchanged)
{
    arena_free_list list;
    list.reset(128);
    const std::vector<freeBlock> before = snapshot(list);

    uint start = 0;
    EXPECT_FALSE(list.reserve(129, start));
    EXPECT_FALSE(list.reserve((std::numeric_limits<uint>::max)(), start));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
}

TEST(arenaFreeList_p1_reserve_exactly_capacity_succeeds_then_arena_is_full)
{
    arena_free_list list;
    list.reset(128);

    uint start = 999;
    EXPECT_TRUE(list.reserve(128, start));
    EXPECT_EQ(start, 0u);
    EXPECT_EQ(list.usedBytes(), 128u);
    EXPECT_EQ(list.blockCount(), 0u);
    EXPECT_FALSE(list.reserve(1, start));
    EXPECT_TRUE(structurallySound(list));
}

TEST(arenaFreeList_p1_reserve_capacity_minus_one_leaves_single_byte_tail)
{
    arena_free_list list;
    list.reset(128);

    uint start = 0;
    EXPECT_TRUE(list.reserve(127, start));
    EXPECT_EQ(start, 0u);
    uint tailStart = 0;
    uint tailSize = 0;
    EXPECT_EQ(list.blockCount(), 1u);
    EXPECT_TRUE(list.block(0, tailStart, tailSize));
    EXPECT_EQ(tailStart, 127u);
    EXPECT_EQ(tailSize, 1u);
}

TEST(arenaFreeList_p6_block_out_of_range_reports_false_and_zeroes_outputs)
{
    arena_free_list list;
    list.reset(64);

    const uint probes[] = { list.blockCount(), list.blockCount() + 1, 1000000u, (std::numeric_limits<uint>::max)() };
    for (const uint probe : probes)
    {
        uint start = 123;
        uint size = 456;
        EXPECT_FALSE(list.block(probe, start, size));
        EXPECT_EQ(start, 0u);
        EXPECT_EQ(size, 0u);
    }
}

TEST(arenaFreeList_p6_release_zero_size_fails_and_leaves_state_unchanged)
{
    arena_free_list list;
    list.reset(128);
    uint start = 0;
    EXPECT_TRUE(list.reserve(32, start));
    const std::vector<freeBlock> before = snapshot(list);

    EXPECT_FALSE(list.release(0, 0));
    EXPECT_FALSE(list.release(16, 0));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
}

TEST(arenaFreeList_p6_release_past_capacity_fails_and_leaves_state_unchanged)
{
    arena_free_list list;
    list.reset(128);
    uint start = 0;
    EXPECT_TRUE(list.reserve(128, start));
    const std::vector<freeBlock> before = snapshot(list);

    EXPECT_FALSE(list.release(0, 129));
    EXPECT_FALSE(list.release(128, 1));
    EXPECT_FALSE(list.release(127, 2));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
    EXPECT_EQ(list.usedBytes(), 128u);
}

TEST(arenaFreeList_p6_release_start_plus_size_wraparound_in_32_bits_is_rejected)
{
    arena_free_list list;
    list.reset((std::numeric_limits<uint>::max)());
    uint start = 0;
    EXPECT_TRUE(list.reserve((std::numeric_limits<uint>::max)(), start));
    const std::vector<freeBlock> before = snapshot(list);

    EXPECT_FALSE(list.release(0xFFFFFFF0u, 0x20u));
    EXPECT_FALSE(list.release((std::numeric_limits<uint>::max)(), (std::numeric_limits<uint>::max)()));
    EXPECT_FALSE(list.release(0x80000000u, 0x80000001u));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
}

TEST(arenaFreeList_p3_double_free_second_release_fails_and_state_is_identical)
{
    arena_free_list list;
    list.reset(256);
    uint a = 0;
    uint b = 0;
    uint c = 0;
    EXPECT_TRUE(list.reserve(64, a));
    EXPECT_TRUE(list.reserve(64, b));
    EXPECT_TRUE(list.reserve(64, c));

    EXPECT_TRUE(list.release(b, 64));
    const std::vector<freeBlock> afterFirst = snapshot(list);
    const uint usedAfterFirst = list.usedBytes();

    EXPECT_FALSE(list.release(b, 64));
    EXPECT_FALSE(list.release(b, 64));
    EXPECT_TRUE(sameBlocks(afterFirst, snapshot(list)));
    EXPECT_EQ(list.usedBytes(), usedAfterFirst);
    EXPECT_TRUE(structurallySound(list));
}

TEST(arenaFreeList_p6_release_overlapping_free_region_is_rejected_without_mutation)
{
    arena_free_list list;
    list.reset(256);
    uint a = 0;
    uint b = 0;
    EXPECT_TRUE(list.reserve(64, a));
    EXPECT_TRUE(list.reserve(64, b));
    EXPECT_EQ(a, 0u);
    EXPECT_EQ(b, 64u);
    const std::vector<freeBlock> before = snapshot(list);

    EXPECT_FALSE(list.release(b, 65));
    EXPECT_FALSE(list.release(127, 2));
    EXPECT_FALSE(list.release(255, 1));
    EXPECT_FALSE(list.release(b + 32, 64));
    EXPECT_FALSE(list.release(128, 1));
    EXPECT_FALSE(list.release(0, 256));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
}

TEST(arenaFreeList_p1_release_of_live_bytes_is_byte_granular_and_coalesces)
{
    arena_free_list list;
    list.reset(256);
    uint a = 0;
    uint b = 0;
    EXPECT_TRUE(list.reserve(64, a));
    EXPECT_TRUE(list.reserve(64, b));
    EXPECT_TRUE(structurallySound(list));

    EXPECT_TRUE(list.release(a, 65));
    EXPECT_EQ(list.usedBytes(), 63u);
    EXPECT_TRUE(structurallySound(list));

    const std::vector<freeBlock> afterFirst = snapshot(list);
    EXPECT_FALSE(list.release(a, 1));
    EXPECT_TRUE(sameBlocks(afterFirst, snapshot(list)));
    EXPECT_EQ(list.usedBytes(), 63u);
    EXPECT_TRUE(structurallySound(list));

    EXPECT_TRUE(list.release(65, 63));
    EXPECT_EQ(list.usedBytes(), 0u);
    EXPECT_EQ(list.blockCount(), 1u);
    uint start = 1;
    uint size = 0;
    EXPECT_TRUE(list.block(0, start, size));
    EXPECT_EQ(start, 0u);
    EXPECT_EQ(size, 256u);
    EXPECT_TRUE(structurallySound(list));
}

TEST(arenaFreeList_p6_release_on_never_reserved_fresh_arena_is_rejected)
{
    arena_free_list list;
    list.reset(256);
    const std::vector<freeBlock> before = snapshot(list);

    EXPECT_FALSE(list.release(0, 1));
    EXPECT_FALSE(list.release(100, 50));
    EXPECT_FALSE(list.release(255, 1));
    EXPECT_TRUE(sameBlocks(before, snapshot(list)));
    EXPECT_EQ(list.usedBytes(), 0u);
}

TEST(arenaFreeList_p1_release_coalesces_with_previous_next_and_both_neighbours)
{
    arena_free_list list;
    list.reset(400);
    uint s[5] = {};
    for (uint& slot : s)
    {
        EXPECT_TRUE(list.reserve(80, slot));
    }
    EXPECT_EQ(list.blockCount(), 0u);

    EXPECT_TRUE(list.release(s[1], 80));
    EXPECT_EQ(list.blockCount(), 1u);
    EXPECT_TRUE(list.release(s[3], 80));
    EXPECT_EQ(list.blockCount(), 2u);

    EXPECT_TRUE(list.release(s[2], 80));
    EXPECT_EQ(list.blockCount(), 1u);
    uint start = 0;
    uint size = 0;
    EXPECT_TRUE(list.block(0, start, size));
    EXPECT_EQ(start, s[1]);
    EXPECT_EQ(size, 240u);

    EXPECT_TRUE(list.release(s[0], 80));
    EXPECT_EQ(list.blockCount(), 1u);
    EXPECT_TRUE(list.release(s[4], 80));
    EXPECT_EQ(list.blockCount(), 1u);
    EXPECT_TRUE(list.block(0, start, size));
    EXPECT_EQ(start, 0u);
    EXPECT_EQ(size, 400u);
    EXPECT_EQ(list.usedBytes(), 0u);
}

TEST(arenaFreeList_p1_reserve_is_first_fit_lowest_address)
{
    arena_free_list list;
    list.reset(300);
    uint a = 0;
    uint b = 0;
    uint c = 0;
    uint d = 0;
    EXPECT_TRUE(list.reserve(50, a));
    EXPECT_TRUE(list.reserve(100, b));
    EXPECT_TRUE(list.reserve(50, c));
    EXPECT_TRUE(list.reserve(100, d));
    EXPECT_TRUE(list.release(b, 100));
    EXPECT_TRUE(list.release(d, 100));

    uint start = 0;
    EXPECT_TRUE(list.reserve(100, start));
    EXPECT_EQ(start, b);
    EXPECT_TRUE(list.reserve(1, start));
    EXPECT_EQ(start, d);
}

TEST(arenaFreeList_p3_reset_twice_with_same_capacity_gives_identical_state)
{
    arena_free_list list;
    list.reset(512);
    uint start = 0;
    EXPECT_TRUE(list.reserve(100, start));
    list.reset(512);
    const std::vector<freeBlock> once = snapshot(list);
    list.reset(512);
    EXPECT_TRUE(sameBlocks(once, snapshot(list)));
    EXPECT_EQ(list.usedBytes(), 0u);
    EXPECT_EQ(list.blockCount(), 1u);
}

TEST(arenaFreeList_p1_invariant_random_reserve_release_sequences_preserve_structure_and_never_overlap)
{
    const auto property = [] {
        const uint capacity = *rc::gen::inRange<uint>(1, 2049);
        arena_free_list list;
        list.reset(capacity);

        std::vector<liveBlock> live;
        std::uint64_t liveBytes = 0;
        const int steps = *rc::gen::inRange<int>(0, 200);

        for (int step = 0; step < steps; ++step)
        {
            const bool doReserve = live.empty() || *rc::gen::inRange<int>(0, 100) < 55;
            if (doReserve)
            {
                const uint size = *rc::gen::inRange<uint>(1, capacity + 2);
                const std::vector<freeBlock> before = snapshot(list);
                bool anyFits = false;
                uint expectedStart = 0;
                for (const freeBlock& b : before)
                {
                    if (b.size >= size)
                    {
                        anyFits = true;
                        expectedStart = b.start;
                        break;
                    }
                }

                uint start = 0;
                const bool ok = list.reserve(size, start);
                RC_ASSERT(ok == anyFits);
                if (ok)
                {
                    RC_ASSERT(start == expectedStart);
                    const liveBlock fresh{ start, size };
                    for (const liveBlock& other : live)
                    {
                        RC_ASSERT(!bytesOverlap(fresh, other));
                    }
                    RC_ASSERT(static_cast<std::uint64_t>(start) + size <= capacity);
                    live.push_back(fresh);
                    liveBytes += size;
                }
                else
                {
                    RC_ASSERT(sameBlocks(before, snapshot(list)));
                }
            }
            else
            {
                const size_t index = *rc::gen::inRange<size_t>(0, live.size());
                const liveBlock victim = live[index];
                RC_ASSERT(list.release(victim.start, victim.size));
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(index));
                liveBytes -= victim.size;
            }

            RC_ASSERT(structurallySound(list));
            RC_ASSERT(list.usedBytes() == liveBytes);
            RC_ASSERT(list.capacity() == capacity);
        }
    };
    EXPECT_PROPERTY("random reserve/release sequences preserve free-list invariants", property);
}

TEST(arenaFreeList_p6_fuzz_arbitrary_release_calls_never_corrupt_structure)
{
    const auto property = [] {
        const uint capacity = *rc::gen::inRange<uint>(0, 1025);
        arena_free_list list;
        list.reset(capacity);

        const int steps = *rc::gen::inRange<int>(0, 150);
        for (int step = 0; step < steps; ++step)
        {
            const int op = *rc::gen::inRange<int>(0, 3);
            if (op == 0)
            {
                uint start = 0;
                list.reserve(*rc::gen::inRange<uint>(0, 1100), start);
            }
            else if (op == 1)
            {
                list.release(*rc::gen::inRange<uint>(0, 1100), *rc::gen::inRange<uint>(0, 1100));
            }
            else
            {
                list.release(*rc::gen::arbitrary<uint>(), *rc::gen::arbitrary<uint>());
            }

            RC_ASSERT(structurallySound(list));
            RC_ASSERT(list.capacity() == capacity);
        }
    };
    EXPECT_PROPERTY("arbitrary reserve/release calls keep the free list well formed", property);
}

TEST(arenaFreeList_p3_idempotent_rejected_release_never_changes_state)
{
    const auto property = [] {
        const uint capacity = *rc::gen::inRange<uint>(1, 1025);
        arena_free_list list;
        list.reset(capacity);

        std::vector<liveBlock> live;
        const int allocations = *rc::gen::inRange<int>(1, 30);
        for (int i = 0; i < allocations; ++i)
        {
            uint start = 0;
            const uint size = *rc::gen::inRange<uint>(1, 64);
            if (list.reserve(size, start))
            {
                live.push_back({ start, size });
            }
        }
        RC_PRE(!live.empty());

        const liveBlock victim = live[*rc::gen::inRange<size_t>(0, live.size())];
        RC_ASSERT(list.release(victim.start, victim.size));
        const std::vector<freeBlock> afterFirst = snapshot(list);
        const uint usedAfterFirst = list.usedBytes();

        const int repeats = *rc::gen::inRange<int>(1, 6);
        for (int i = 0; i < repeats; ++i)
        {
            RC_ASSERT(!list.release(victim.start, victim.size));
            RC_ASSERT(sameBlocks(afterFirst, snapshot(list)));
            RC_ASSERT(list.usedBytes() == usedAfterFirst);
        }
    };
    EXPECT_PROPERTY("releasing the same block repeatedly is rejected with no state change", property);
}

TEST(arenaFreeList_p5_commutativity_release_permutation_coalesces_to_single_block)
{
    const auto property = [] {
        const int blockTotal = *rc::gen::inRange<int>(1, 40);
        std::vector<uint> sizes;
        std::uint64_t sum = 0;
        for (int i = 0; i < blockTotal; ++i)
        {
            const uint size = *rc::gen::inRange<uint>(1, 65);
            sizes.push_back(size);
            sum += size;
        }
        const uint tail = *rc::gen::inRange<uint>(0, 33);
        const uint capacity = static_cast<uint>(sum) + tail;

        arena_free_list list;
        list.reset(capacity);

        std::vector<liveBlock> live;
        for (const uint size : sizes)
        {
            uint start = 0;
            RC_ASSERT(list.reserve(size, start));
            live.push_back({ start, size });
        }
        RC_ASSERT(list.usedBytes() == sum);

        std::vector<size_t> order(live.size());
        std::iota(order.begin(), order.end(), size_t{ 0 });
        const std::vector<std::uint32_t> keys = *rc::gen::container<std::vector<std::uint32_t>>(live.size(), rc::gen::arbitrary<std::uint32_t>());
        std::stable_sort(order.begin(), order.end(), [&keys](size_t a, size_t b) { return keys[a] < keys[b]; });

        for (const size_t index : order)
        {
            RC_ASSERT(list.release(live[index].start, live[index].size));
            RC_ASSERT(structurallySound(list));
        }

        RC_ASSERT(list.usedBytes() == 0u);
        RC_ASSERT(list.blockCount() == 1u);
        uint start = 0;
        uint size = 0;
        RC_ASSERT(list.block(0, start, size));
        RC_ASSERT(start == 0u);
        RC_ASSERT(size == capacity);
    };
    EXPECT_PROPERTY("any release order coalesces back to one block with usedBytes zero", property);
}

TEST(arenaFreeList_p5_commutativity_two_permutations_yield_identical_final_state)
{
    const auto property = [] {
        const int blockTotal = *rc::gen::inRange<int>(2, 30);
        std::vector<uint> sizes;
        std::uint64_t sum = 0;
        for (int i = 0; i < blockTotal; ++i)
        {
            const uint size = *rc::gen::inRange<uint>(1, 40);
            sizes.push_back(size);
            sum += size;
        }
        const uint capacity = static_cast<uint>(sum) + 16u;
        const size_t releaseCount = *rc::gen::inRange<size_t>(0, sizes.size() + 1);

        const auto buildAndRelease = [&sizes, capacity, releaseCount](const std::vector<std::uint32_t>& keys) {
            arena_free_list list;
            list.reset(capacity);
            std::vector<liveBlock> live;
            for (const uint size : sizes)
            {
                uint start = 0;
                RC_ASSERT(list.reserve(size, start));
                live.push_back({ start, size });
            }
            std::vector<size_t> order(live.size());
            std::iota(order.begin(), order.end(), size_t{ 0 });
            std::stable_sort(order.begin(), order.end(), [&keys](size_t a, size_t b) { return keys[a] < keys[b]; });
            std::vector<bool> chosen(live.size(), false);
            for (size_t i = 0; i < live.size(); ++i)
            {
                if (i < releaseCount)
                {
                    chosen[i] = true;
                }
            }
            for (const size_t index : order)
            {
                if (chosen[index])
                {
                    RC_ASSERT(list.release(live[index].start, live[index].size));
                }
            }
            return snapshot(list);
        };

        const std::vector<std::uint32_t> keysA = *rc::gen::container<std::vector<std::uint32_t>>(sizes.size(), rc::gen::arbitrary<std::uint32_t>());
        const std::vector<std::uint32_t> keysB = *rc::gen::container<std::vector<std::uint32_t>>(sizes.size(), rc::gen::arbitrary<std::uint32_t>());
        RC_ASSERT(sameBlocks(buildAndRelease(keysA), buildAndRelease(keysB)));
    };
    EXPECT_PROPERTY("final free list depends only on the set of released blocks, not their order", property);
}
