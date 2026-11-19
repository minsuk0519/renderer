#include "property_harness.hpp"

#include <render/buffer.hpp>
#include <system/config.hpp>

#include <cstdint>
#include <limits>
#include <set>
#include <vector>

namespace
{
    constexpr uint idInvalid = BUFFER_ID_INVALID;
}

TEST(bufferIdSource_p1_invalid_sentinel_is_uint_max)
{
    EXPECT_EQ(BUFFER_ID_INVALID, (std::numeric_limits<uint>::max)());
}

TEST(bufferIdSource_p1_reset_zero_issues_consecutive_ids_from_zero)
{
    buffer_id_source source;
    source.reset(0);

    for (uint expected = 0; expected < 1000; ++expected)
    {
        uint id = idInvalid;
        EXPECT_TRUE(source.acquire(id));
        EXPECT_EQ(id, expected);
    }
}

TEST(bufferIdSource_p1_long_sequence_never_wraps_at_16_bit_boundary_or_collides)
{
    buffer_id_source source;
    source.reset(0);

    const uint total = 200000;
    uint previous = 0;
    for (uint i = 0; i < total; ++i)
    {
        uint id = idInvalid;
        const bool ok = source.acquire(id);
        EXPECT_TRUE(ok);
        EXPECT_NE(id, idInvalid);
        EXPECT_EQ(id, i);
        if (i > 0)
        {
            EXPECT_TRUE(id > previous);
        }
        previous = id;
        if (!ok)
        {
            break;
        }
    }
}

TEST(bufferIdSource_p6_reset_to_invalid_makes_acquire_fail_immediately_and_clears_output)
{
    buffer_id_source source;
    source.reset(idInvalid);

    uint id = 12345;
    EXPECT_FALSE(source.acquire(id));
    EXPECT_EQ(id, idInvalid);
    id = 7;
    EXPECT_FALSE(source.acquire(id));
    EXPECT_EQ(id, idInvalid);
}

TEST(bufferIdSource_p6_reset_to_invalid_minus_one_issues_exactly_one_id_then_exhausts)
{
    buffer_id_source source;
    source.reset(idInvalid - 1);

    uint id = 0;
    EXPECT_TRUE(source.acquire(id));
    EXPECT_EQ(id, idInvalid - 1);
    EXPECT_NE(id, idInvalid);

    EXPECT_FALSE(source.acquire(id));
    EXPECT_EQ(id, idInvalid);
    EXPECT_FALSE(source.acquire(id));
    EXPECT_EQ(id, idInvalid);
}

TEST(bufferIdSource_p3_idempotent_repeated_acquire_after_exhaustion_keeps_failing)
{
    buffer_id_source source;
    source.reset(idInvalid - 2);

    uint id = 0;
    EXPECT_TRUE(source.acquire(id));
    EXPECT_TRUE(source.acquire(id));
    for (int i = 0; i < 100; ++i)
    {
        id = 0;
        EXPECT_FALSE(source.acquire(id));
        EXPECT_EQ(id, idInvalid);
    }
}

TEST(bufferIdSource_p3_reset_after_exhaustion_restarts_the_sequence)
{
    buffer_id_source source;
    source.reset(idInvalid);
    uint id = 0;
    EXPECT_FALSE(source.acquire(id));

    source.reset(5);
    EXPECT_TRUE(source.acquire(id));
    EXPECT_EQ(id, 5u);
    EXPECT_TRUE(source.acquire(id));
    EXPECT_EQ(id, 6u);

    source.reset(5);
    EXPECT_TRUE(source.acquire(id));
    EXPECT_EQ(id, 5u);
}

TEST(bufferIdSource_p1_invariant_random_first_id_and_count_issue_consecutive_unique_valid_ids)
{
    const auto property = [] {
        const uint firstId = *rc::gen::arbitrary<uint>();
        const int count = *rc::gen::inRange<int>(0, 400);

        buffer_id_source source;
        source.reset(firstId);

        std::set<uint> seen;
        std::uint64_t expectedNext = firstId;
        for (int i = 0; i < count; ++i)
        {
            uint id = 99;
            const bool ok = source.acquire(id);
            const bool shouldSucceed = expectedNext < idInvalid;
            RC_ASSERT(ok == shouldSucceed);
            if (ok)
            {
                RC_ASSERT(id != idInvalid);
                RC_ASSERT(id == expectedNext);
                RC_ASSERT(seen.insert(id).second);
                ++expectedNext;
            }
            else
            {
                RC_ASSERT(id == idInvalid);
            }
        }
    };
    EXPECT_PROPERTY("acquire issues consecutive unique valid ids from any firstId and never wraps", property);
}

TEST(bufferIdSource_p6_fuzz_near_exhaustion_succeeds_exactly_invalid_minus_first_times)
{
    const auto property = [] {
        const uint distance = *rc::gen::inRange<uint>(0, 300);
        const uint firstId = idInvalid - distance;

        buffer_id_source source;
        source.reset(firstId);

        uint successes = 0;
        uint lastId = idInvalid;
        for (uint i = 0; i < distance + 20; ++i)
        {
            uint id = 0;
            if (source.acquire(id))
            {
                RC_ASSERT(id != idInvalid);
                lastId = id;
                ++successes;
            }
            else
            {
                RC_ASSERT(id == idInvalid);
            }
        }
        RC_ASSERT(successes == distance);
        if (distance > 0)
        {
            RC_ASSERT(lastId == idInvalid - 1);
        }
    };
    EXPECT_PROPERTY("an id source near the top issues exactly the remaining valid ids", property);
}

TEST(bufferIdSource_p1_invariant_reset_makes_state_independent_of_history)
{
    const auto property = [] {
        const uint firstId = *rc::gen::arbitrary<uint>();
        const int warmup = *rc::gen::inRange<int>(0, 100);
        const int probe = *rc::gen::inRange<int>(0, 100);

        buffer_id_source fresh;
        fresh.reset(firstId);

        buffer_id_source used;
        used.reset(*rc::gen::arbitrary<uint>());
        for (int i = 0; i < warmup; ++i)
        {
            uint ignored = 0;
            used.acquire(ignored);
        }
        used.reset(firstId);

        for (int i = 0; i < probe; ++i)
        {
            uint a = 1;
            uint b = 2;
            const bool okA = fresh.acquire(a);
            const bool okB = used.acquire(b);
            RC_ASSERT(okA == okB);
            RC_ASSERT(a == b);
        }
    };
    EXPECT_PROPERTY("reset erases all history of earlier acquires", property);
}
