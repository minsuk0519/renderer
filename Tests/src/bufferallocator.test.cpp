#include "test_harness.hpp"

#include <render/buffer.hpp>
#include <system/config.hpp>

#include <limits>

namespace
{
    void ensureAllocatorInitialized()
    {
        static bool initialized = false;
        if (!initialized)
        {
            e_globBufAllocator.init();
            initialized = true;
        }
    }

#if ENGINE_DEBUG_RESOURCEVIEW
    bool freeListIsSortedDisjointAndWithinCapacity()
    {
        const uint blockCount = e_globBufAllocator.getFreeBlockCount();
        uint previousEnd = 0;
        for (uint i = 0; i < blockCount; ++i)
        {
            uint start = 0;
            uint size = 0;
            e_globBufAllocator.getFreeBlock(i, start, size);
            if (start < previousEnd)
            {
                return false;
            }
            if (static_cast<unsigned long long>(start) + size > static_cast<unsigned long long>(e_globBufAllocator.getArenaCapacity()))
            {
                return false;
            }
            previousEnd = start + size;
        }
        return true;
    }
#endif
}

TEST(bufferAllocator_p6_degenerate_free_unknown_index_on_fresh_allocator_does_not_crash)
{
    ensureAllocatorInitialized();

    const uint indices[] = { 0u, 1u, 7u, 65535u, 65536u, (std::numeric_limits<uint>::max)() };
    for (const uint index : indices)
    {
        e_globBufAllocator.free(index);
    }

#if ENGINE_DEBUG_RESOURCEVIEW
    EXPECT_EQ(e_globBufAllocator.getArenaUsed(), 0u);
    EXPECT_EQ(e_globBufAllocator.getFreeBlockCount(), 1u);
    EXPECT_TRUE(freeListIsSortedDisjointAndWithinCapacity());
#endif
}

TEST(bufferAllocator_p3_idempotent_repeated_free_of_unknown_index_leaves_arena_unchanged)
{
    ensureAllocatorInitialized();

#if ENGINE_DEBUG_RESOURCEVIEW
    const uint usedBefore = e_globBufAllocator.getArenaUsed();
    const uint blocksBefore = e_globBufAllocator.getFreeBlockCount();
    uint startBefore = 0;
    uint sizeBefore = 0;
    e_globBufAllocator.getFreeBlock(0, startBefore, sizeBefore);
#endif

    const uint unknownIndex = 4242u;
    for (int i = 0; i < 50; ++i)
    {
        e_globBufAllocator.free(unknownIndex);
    }

#if ENGINE_DEBUG_RESOURCEVIEW
    uint startAfter = 0;
    uint sizeAfter = 0;
    e_globBufAllocator.getFreeBlock(0, startAfter, sizeAfter);
    EXPECT_EQ(e_globBufAllocator.getArenaUsed(), usedBefore);
    EXPECT_EQ(e_globBufAllocator.getFreeBlockCount(), blocksBefore);
    EXPECT_EQ(startAfter, startBefore);
    EXPECT_EQ(sizeAfter, sizeBefore);
#endif
}

#if ENGINE_DEBUG_RESOURCEVIEW
TEST(bufferAllocator_p1_invariant_getFreeBlock_out_of_range_index_reports_empty_block)
{
    ensureAllocatorInitialized();

    const uint count = e_globBufAllocator.getFreeBlockCount();
    const uint probes[] = { count, count + 1, 1000000u, (std::numeric_limits<uint>::max)() };
    for (const uint probe : probes)
    {
        uint start = 123;
        uint size = 456;
        e_globBufAllocator.getFreeBlock(probe, start, size);
        EXPECT_EQ(start, 0u);
        EXPECT_EQ(size, 0u);
    }
}

TEST(bufferAllocator_p1_invariant_free_plus_used_equals_capacity_on_fresh_arena)
{
    ensureAllocatorInitialized();

    uint freeTotal = 0;
    const uint count = e_globBufAllocator.getFreeBlockCount();
    for (uint i = 0; i < count; ++i)
    {
        uint start = 0;
        uint size = 0;
        e_globBufAllocator.getFreeBlock(i, start, size);
        freeTotal += size;
    }
    EXPECT_EQ(freeTotal + e_globBufAllocator.getArenaUsed(), e_globBufAllocator.getArenaCapacity());
}
#endif
