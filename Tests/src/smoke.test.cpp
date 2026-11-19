#include "test_harness.hpp"

#include <render/buffer.hpp>
#include <system/config.hpp>

TEST(smoke_harness_and_engine_link)
{
    EXPECT_EQ(MAX_OBJECTS, 256u);
    EXPECT_EQ(buf::estimateBufferSize(0), static_cast<uint>(sizeof(buffer)));
}
