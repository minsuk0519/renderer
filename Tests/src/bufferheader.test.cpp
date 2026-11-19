#include "test_harness.hpp"

#include <render/buffer.hpp>
#include <system/config.hpp>

#include <cstddef>
#include <limits>
#include <type_traits>

TEST(bufferHeader_p1_invariant_header_is_four_32_bit_words)
{
    EXPECT_EQ(sizeof(buffer_header), 16u);
    EXPECT_EQ(sizeof(buffer_header::packed_data), 4u);
}

TEST(bufferHeader_p1_invariant_buffer_id_is_a_plain_32_bit_member_after_total_size)
{
    EXPECT_TRUE((std::is_same_v<decltype(buffer_header::bufferId), uint>));
    EXPECT_EQ(offsetof(buffer_header, bufferId), 8u);
}

TEST(bufferHeader_p4_round_trip_buffer_id_survives_values_beyond_16_bits)
{
    const uint ids[] = { 0u, 1u, 65535u, 65536u, 65537u, 0x7FFFFFFFu, BUFFER_ID_INVALID - 1u, BUFFER_ID_INVALID };
    for (const uint original : ids)
    {
        buffer_header header{};
        header.packedData.allocated = 1;
        header.packedData.viewFlags = 0xFF;
        header.packedData.stride = 0xF;
        header.packedData.texture = 1;
        header.packedData.lifetime = 1;
        header.packedData.external = 1;
        header.dataSize = 0xAAAAAAAAu;
        header.totalSize = 0x55555555u;

        header.bufferId = original;

        EXPECT_EQ(header.bufferId, original);
        EXPECT_EQ(header.dataSize, 0xAAAAAAAAu);
        EXPECT_EQ(header.totalSize, 0x55555555u);
        EXPECT_EQ(header.packedData.allocated, 1u);
        EXPECT_EQ(header.packedData.viewFlags, 0xFFu);
        EXPECT_EQ(header.packedData.stride, 0xFu);
        EXPECT_EQ(header.packedData.texture, 1u);
        EXPECT_EQ(header.packedData.lifetime, 1u);
        EXPECT_EQ(header.packedData.external, 1u);
    }
}

TEST(bufferHeader_p1_invariant_buffer_is_large_enough_to_hold_its_header)
{
    EXPECT_TRUE(sizeof(buffer) >= sizeof(buffer_header));
}
