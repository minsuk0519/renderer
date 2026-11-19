#include "property_harness.hpp"

#include <render/buffer.hpp>

#include <cstdint>
#include <limits>

namespace
{
    bool referenceValid(std::uint64_t capacity, std::uint32_t offset, std::uint32_t size)
    {
        if (size > capacity)
        {
            return false;
        }
        return offset <= capacity - size;
    }

    constexpr std::uint32_t u32Max = (std::numeric_limits<std::uint32_t>::max)();
    constexpr std::uint64_t u64Max = (std::numeric_limits<std::uint64_t>::max)();
}

TEST(uploadRangeValid_p1_boundary_exact_fit_accepted_one_over_rejected)
{
    EXPECT_TRUE(buf::uploadRangeValid(100, 0, 100));
    EXPECT_TRUE(buf::uploadRangeValid(100, 100, 0));
    EXPECT_TRUE(buf::uploadRangeValid(100, 40, 60));
    EXPECT_FALSE(buf::uploadRangeValid(100, 0, 101));
    EXPECT_FALSE(buf::uploadRangeValid(100, 101, 0));
    EXPECT_FALSE(buf::uploadRangeValid(100, 41, 60));
    EXPECT_FALSE(buf::uploadRangeValid(100, 40, 61));
}

TEST(uploadRangeValid_p6_zero_capacity_only_accepts_empty_range_at_zero)
{
    EXPECT_TRUE(buf::uploadRangeValid(0, 0, 0));
    EXPECT_FALSE(buf::uploadRangeValid(0, 0, 1));
    EXPECT_FALSE(buf::uploadRangeValid(0, 1, 0));
    EXPECT_FALSE(buf::uploadRangeValid(0, u32Max, u32Max));
}

TEST(uploadRangeValid_p6_offset_plus_size_32_bit_overflow_is_rejected_for_small_capacity)
{
    EXPECT_FALSE(buf::uploadRangeValid(16, u32Max, 17));
    EXPECT_FALSE(buf::uploadRangeValid(16, u32Max, 2));
    EXPECT_FALSE(buf::uploadRangeValid(16, 0x80000000u, 0x80000000u));
    EXPECT_FALSE(buf::uploadRangeValid(1, u32Max, u32Max));
    EXPECT_FALSE(buf::uploadRangeValid(0xFFFFFFFFull, u32Max, 1));
}

TEST(uploadRangeValid_p1_64_bit_capacity_accepts_ranges_beyond_32_bit_sum)
{
    EXPECT_TRUE(buf::uploadRangeValid(0x1FFFFFFFEull, u32Max, u32Max));
    EXPECT_FALSE(buf::uploadRangeValid(0x1FFFFFFFDull, u32Max, u32Max));
    EXPECT_TRUE(buf::uploadRangeValid(u64Max, u32Max, u32Max));
    EXPECT_TRUE(buf::uploadRangeValid(u64Max, 0, 0));
}

TEST(uploadRangeValid_p2_metamorphic_shrinking_range_or_growing_capacity_keeps_it_valid)
{
    const auto property = [] {
        const std::uint64_t capacity = *rc::gen::arbitrary<std::uint64_t>();
        const std::uint32_t offset = *rc::gen::arbitrary<std::uint32_t>();
        const std::uint32_t size = *rc::gen::arbitrary<std::uint32_t>();
        RC_PRE(buf::uploadRangeValid(capacity, offset, size));

        const std::uint32_t smallerSize = *rc::gen::inRange<std::uint32_t>(0, size == u32Max ? size : size + 1);
        const std::uint32_t smallerOffset = *rc::gen::inRange<std::uint32_t>(0, offset == u32Max ? offset : offset + 1);
        RC_ASSERT(buf::uploadRangeValid(capacity, offset, smallerSize));
        RC_ASSERT(buf::uploadRangeValid(capacity, smallerOffset, size));
        RC_ASSERT(buf::uploadRangeValid(capacity, smallerOffset, smallerSize));

        const std::uint64_t slack = *rc::gen::arbitrary<std::uint64_t>();
        const std::uint64_t biggerCapacity = capacity > u64Max - slack ? u64Max : capacity + slack;
        RC_ASSERT(buf::uploadRangeValid(biggerCapacity, offset, size));
    };
    EXPECT_PROPERTY("valid ranges stay valid when shrunk or when capacity grows", property);
}

TEST(uploadRangeValid_p1_invariant_matches_overflow_free_reference_for_arbitrary_triples)
{
    const auto property = [] {
        const std::uint64_t capacity = *rc::gen::arbitrary<std::uint64_t>();
        const std::uint32_t offset = *rc::gen::arbitrary<std::uint32_t>();
        const std::uint32_t size = *rc::gen::arbitrary<std::uint32_t>();
        RC_ASSERT(buf::uploadRangeValid(capacity, offset, size) == referenceValid(capacity, offset, size));
    };
    EXPECT_PROPERTY("uploadRangeValid agrees with subtraction-based reference", property);
}

TEST(uploadRangeValid_p6_fuzz_near_boundary_triples_agree_with_reference)
{
    const auto property = [] {
        const std::uint32_t offset = *rc::gen::oneOf(rc::gen::arbitrary<std::uint32_t>(), rc::gen::inRange<std::uint32_t>(u32Max - 4, u32Max), rc::gen::inRange<std::uint32_t>(0, 5));
        const std::uint32_t size = *rc::gen::oneOf(rc::gen::arbitrary<std::uint32_t>(), rc::gen::inRange<std::uint32_t>(u32Max - 4, u32Max), rc::gen::inRange<std::uint32_t>(0, 5));
        const std::int64_t delta = *rc::gen::inRange<std::int64_t>(-3, 4);
        const std::uint64_t sum = static_cast<std::uint64_t>(offset) + size;
        const std::uint64_t capacity = (delta < 0 && sum < static_cast<std::uint64_t>(-delta)) ? 0 : sum + static_cast<std::uint64_t>(delta);
        RC_ASSERT(buf::uploadRangeValid(capacity, offset, size) == referenceValid(capacity, offset, size));
        RC_ASSERT(buf::uploadRangeValid(sum, offset, size));
        if (sum > 0)
        {
            RC_ASSERT(!buf::uploadRangeValid(sum - 1, offset, size));
        }
    };
    EXPECT_PROPERTY("capacity within +-3 of offset+size is classified exactly", property);
}
