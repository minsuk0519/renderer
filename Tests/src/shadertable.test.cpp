#include "property_harness.hpp"

#include <render/shader.hpp>

#include <limits>

TEST(shaders_p6_getShader_on_empty_table_returns_null_for_boundary_indices)
{
    shaders::cleanup();

    const uint indices[] = { 0u, 1u, 36u, 37u, 255u, 65535u, 65536u, 0x7FFFFFFFu, (std::numeric_limits<uint>::max)() - 1u, (std::numeric_limits<uint>::max)() };
    for (const uint index : indices)
    {
        EXPECT_TRUE(shaders::getShader(index) == nullptr);
    }
}

TEST(shaders_p6_fuzz_getShader_with_arbitrary_index_on_empty_table_never_crashes_and_returns_null)
{
    shaders::cleanup();

    const auto property = [] {
        const uint index = *rc::gen::arbitrary<uint>();
        RC_ASSERT(shaders::getShader(index) == nullptr);
    };
    EXPECT_PROPERTY("getShader on an empty table returns null for any index", property);
}

TEST(shaders_p3_idempotent_cleanup_called_repeatedly_leaves_table_empty)
{
    shaders::cleanup();
    shaders::cleanup();
    shaders::cleanup();

    EXPECT_TRUE(shaders::getShader(0) == nullptr);
    EXPECT_TRUE(shaders::getShader((std::numeric_limits<uint>::max)()) == nullptr);
}

TEST(shaders_p3_idempotent_cleanup_on_never_loaded_table_does_not_crash)
{
    for (int i = 0; i < 10; ++i)
    {
        shaders::cleanup();
    }
    EXPECT_TRUE(shaders::getShader(0) == nullptr);
}
