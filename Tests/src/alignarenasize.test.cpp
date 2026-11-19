#include "property_harness.hpp"

#include <render/buffer.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
    constexpr std::uint64_t alignment = BUFFER_ARENA_ALIGNMENT;
    constexpr std::uint64_t u32Max = (std::numeric_limits<std::uint32_t>::max)();

    struct liveBlock
    {
        uint start;
        uint size;
    };
}

TEST(alignArenaSize_p1_alignment_is_a_nonzero_power_of_two)
{
    EXPECT_TRUE(alignment >= 1u);
    EXPECT_EQ(alignment & (alignment - 1), 0u);
}

TEST(alignArenaSize_p1_rounds_up_known_values)
{
    uint aligned = 99;
    EXPECT_TRUE(buf::alignArenaSize(0, aligned));
    EXPECT_EQ(aligned, 0u);

    EXPECT_TRUE(buf::alignArenaSize(1, aligned));
    EXPECT_EQ(static_cast<std::uint64_t>(aligned), alignment);

    EXPECT_TRUE(buf::alignArenaSize(alignment, aligned));
    EXPECT_EQ(static_cast<std::uint64_t>(aligned), alignment);

    EXPECT_TRUE(buf::alignArenaSize(alignment + 1, aligned));
    EXPECT_EQ(static_cast<std::uint64_t>(aligned), 2 * alignment);

    EXPECT_TRUE(buf::alignArenaSize(alignment - 1, aligned));
    EXPECT_EQ(static_cast<std::uint64_t>(aligned), alignment);
}

TEST(alignArenaSize_p6_raw_above_32_bits_is_rejected_and_output_cleared)
{
    const std::uint64_t rawValues[] = { u32Max + 1, u32Max + 2, 0x200000000ull, 0x7FFFFFFFFFFFFFFFull, (std::numeric_limits<std::uint64_t>::max)() };
    for (const std::uint64_t raw : rawValues)
    {
        uint aligned = 12345;
        EXPECT_FALSE(buf::alignArenaSize(raw, aligned));
        EXPECT_EQ(aligned, 0u);
    }
}

TEST(alignArenaSize_p6_raw_whose_rounded_value_does_not_fit_in_32_bits_is_rejected)
{
    for (std::uint64_t raw = u32Max - (alignment - 1) + 1; raw <= u32Max; ++raw)
    {
        uint aligned = 0;
        EXPECT_FALSE(buf::alignArenaSize(raw, aligned));
    }
}

TEST(alignArenaSize_p1_largest_representable_aligned_value_is_accepted_unchanged)
{
    const std::uint64_t largestAligned = u32Max & ~(alignment - 1);
    uint aligned = 0;
    EXPECT_TRUE(buf::alignArenaSize(largestAligned, aligned));
    EXPECT_EQ(static_cast<std::uint64_t>(aligned), largestAligned);
}

TEST(alignArenaSize_p1_invariant_result_is_multiple_not_smaller_and_less_than_one_step_above)
{
    const auto property = [] {
        const std::uint64_t raw = *rc::gen::oneOf(rc::gen::inRange<std::uint64_t>(0, 100000), rc::gen::inRange<std::uint64_t>(u32Max - 64, u32Max + 64), rc::gen::arbitrary<std::uint64_t>());
        uint aligned = 7;
        const bool ok = buf::alignArenaSize(raw, aligned);
        if (ok)
        {
            RC_ASSERT(raw <= u32Max);
            RC_ASSERT(static_cast<std::uint64_t>(aligned) % alignment == 0u);
            RC_ASSERT(static_cast<std::uint64_t>(aligned) >= raw);
            RC_ASSERT(static_cast<std::uint64_t>(aligned) - raw < alignment);
        }
        else
        {
            RC_ASSERT(aligned == 0u);
        }
    };
    EXPECT_PROPERTY("alignArenaSize returns the smallest aligned size not below raw, or rejects", property);
}

TEST(alignArenaSize_p6_rejection_is_exactly_when_rounded_result_exceeds_32_bits)
{
    const auto property = [] {
        const std::uint64_t raw = *rc::gen::inRange<std::uint64_t>(u32Max - 64, u32Max + 64);
        const std::uint64_t rounded = (raw + (alignment - 1)) & ~(alignment - 1);
        uint aligned = 0;
        const bool ok = buf::alignArenaSize(raw, aligned);
        RC_ASSERT(ok == (rounded <= u32Max));
        if (ok)
        {
            RC_ASSERT(static_cast<std::uint64_t>(aligned) == rounded);
        }
    };
    EXPECT_PROPERTY("alignArenaSize accepts exactly the sizes whose rounded value fits in uint", property);
}

TEST(alignArenaSize_p3_idempotent_aligning_an_aligned_size_changes_nothing)
{
    const auto property = [] {
        const std::uint64_t raw = *rc::gen::inRange<std::uint64_t>(0, u32Max - alignment);
        uint once = 0;
        RC_ASSERT(buf::alignArenaSize(raw, once));
        uint twice = 0;
        RC_ASSERT(buf::alignArenaSize(once, twice));
        RC_ASSERT(once == twice);
    };
    EXPECT_PROPERTY("alignArenaSize(alignArenaSize(x)) == alignArenaSize(x)", property);
}

TEST(alignArenaSize_p2_metamorphic_larger_raw_never_yields_smaller_aligned)
{
    const auto property = [] {
        const std::uint64_t a = *rc::gen::inRange<std::uint64_t>(0, u32Max - alignment);
        const std::uint64_t b = *rc::gen::inRange<std::uint64_t>(0, u32Max - alignment);
        const std::uint64_t smaller = a < b ? a : b;
        const std::uint64_t larger = a < b ? b : a;
        uint alignedSmaller = 0;
        uint alignedLarger = 0;
        RC_ASSERT(buf::alignArenaSize(smaller, alignedSmaller));
        RC_ASSERT(buf::alignArenaSize(larger, alignedLarger));
        RC_ASSERT(alignedSmaller <= alignedLarger);
    };
    EXPECT_PROPERTY("alignArenaSize is monotone", property);
}

TEST(arenaFreeList_p1_invariant_aligned_requests_over_aligned_capacity_always_start_on_alignment)
{
    const auto property = [] {
        const uint capacityUnits = *rc::gen::inRange<uint>(1, 400);
        const uint capacity = static_cast<uint>(capacityUnits * alignment);
        arena_free_list list;
        list.reset(capacity);

        std::vector<liveBlock> live;
        const int steps = *rc::gen::inRange<int>(0, 250);
        for (int step = 0; step < steps; ++step)
        {
            const bool doReserve = live.empty() || *rc::gen::inRange<int>(0, 100) < 55;
            if (doReserve)
            {
                const uint rawSize = *rc::gen::inRange<uint>(1, capacity + 20);
                uint size = 0;
                RC_ASSERT(buf::alignArenaSize(rawSize, size));
                uint start = 1;
                if (list.reserve(size, start))
                {
                    RC_ASSERT(static_cast<std::uint64_t>(start) % alignment == 0u);
                    RC_ASSERT(static_cast<std::uint64_t>(start) + size <= capacity);
                    live.push_back({ start, size });
                }
            }
            else
            {
                const size_t index = *rc::gen::inRange<size_t>(0, live.size());
                RC_ASSERT(list.release(live[index].start, live[index].size));
                live.erase(live.begin() + static_cast<std::ptrdiff_t>(index));
            }

            for (uint i = 0; i < list.blockCount(); ++i)
            {
                uint blockStart = 1;
                uint blockSize = 1;
                RC_ASSERT(list.block(i, blockStart, blockSize));
                RC_ASSERT(static_cast<std::uint64_t>(blockStart) % alignment == 0u);
                RC_ASSERT(static_cast<std::uint64_t>(blockSize) % alignment == 0u);
            }
            RC_ASSERT(static_cast<std::uint64_t>(list.usedBytes()) % alignment == 0u);
        }
    };
    EXPECT_PROPERTY("with aligned capacity and aligned sizes every reserved start and free block stays aligned", property);
}

TEST(arenaFreeList_p5_commutativity_aligned_blocks_released_in_any_order_coalesce_to_one_aligned_block)
{
    const auto property = [] {
        const int count = *rc::gen::inRange<int>(1, 30);
        std::vector<uint> sizes;
        std::uint64_t total = 0;
        for (int i = 0; i < count; ++i)
        {
            const uint rawSize = *rc::gen::inRange<uint>(1, 200);
            uint size = 0;
            RC_ASSERT(buf::alignArenaSize(rawSize, size));
            sizes.push_back(size);
            total += size;
        }
        const uint capacity = static_cast<uint>(total);

        arena_free_list list;
        list.reset(capacity);
        std::vector<liveBlock> live;
        for (const uint size : sizes)
        {
            uint start = 1;
            RC_ASSERT(list.reserve(size, start));
            RC_ASSERT(static_cast<std::uint64_t>(start) % alignment == 0u);
            live.push_back({ start, size });
        }

        std::vector<size_t> order(live.size());
        for (size_t i = 0; i < order.size(); ++i)
        {
            order[i] = i;
        }
        const std::vector<std::uint32_t> keys = *rc::gen::container<std::vector<std::uint32_t>>(live.size(), rc::gen::arbitrary<std::uint32_t>());
        for (size_t i = 0; i + 1 < order.size(); ++i)
        {
            for (size_t j = i + 1; j < order.size(); ++j)
            {
                if (keys[order[j]] < keys[order[i]])
                {
                    const size_t swapped = order[i];
                    order[i] = order[j];
                    order[j] = swapped;
                }
            }
        }

        for (const size_t index : order)
        {
            RC_ASSERT(list.release(live[index].start, live[index].size));
        }
        RC_ASSERT(list.usedBytes() == 0u);
        RC_ASSERT(list.blockCount() == 1u);
        uint start = 1;
        uint size = 0;
        RC_ASSERT(list.block(0, start, size));
        RC_ASSERT(start == 0u);
        RC_ASSERT(size == capacity);
    };
    EXPECT_PROPERTY("aligned blocks released in any order coalesce to the whole aligned arena", property);
}
