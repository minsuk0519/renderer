#include "property_harness.hpp"

#include <render/pipelinestate.hpp>

#include <cstddef>
#include <cstring>
#include <vector>

namespace
{
    constexpr size_t maxTargets = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC makeSentinelDesc()
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.NumRenderTargets = 77;
        for (size_t i = 0; i < maxTargets; ++i)
        {
            desc.RTVFormats[i] = DXGI_FORMAT_R8_UNORM;
        }
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        return desc;
    }

    bool descBytesEqual(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& a, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& b)
    {
        return std::memcmp(&a, &b, sizeof(a)) == 0;
    }

    std::vector<uint> makeFormats(size_t count)
    {
        std::vector<uint> formats;
        for (size_t i = 0; i < count; ++i)
        {
            formats.push_back(static_cast<uint>(2 + (i % 100)));
        }
        return formats;
    }
}

TEST(fillRTVFormats_p1_sizes_zero_to_eight_copy_exactly_and_leave_other_slots_untouched)
{
    for (size_t count = 0; count <= maxTargets; ++count)
    {
        const std::vector<uint> formats = makeFormats(count);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = makeSentinelDesc();

        EXPECT_TRUE(render::fillRTVFormats(formats, desc));
        EXPECT_EQ(static_cast<size_t>(desc.NumRenderTargets), count);
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(static_cast<uint>(desc.RTVFormats[i]), formats[i]);
        }
        for (size_t i = count; i < maxTargets; ++i)
        {
            EXPECT_EQ(static_cast<uint>(desc.RTVFormats[i]), static_cast<uint>(DXGI_FORMAT_R8_UNORM));
        }
        EXPECT_EQ(static_cast<uint>(desc.DSVFormat), static_cast<uint>(DXGI_FORMAT_D32_FLOAT));
    }
}

TEST(fillRTVFormats_p6_sizes_nine_to_sixty_four_are_rejected_and_desc_is_byte_identical)
{
    for (size_t count = maxTargets + 1; count <= 64; ++count)
    {
        const std::vector<uint> formats = makeFormats(count);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = makeSentinelDesc();
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC before = desc;

        EXPECT_FALSE(render::fillRTVFormats(formats, desc));
        EXPECT_TRUE(descBytesEqual(before, desc));
    }
}

TEST(fillRTVFormats_p6_huge_format_list_is_rejected_without_touching_desc)
{
    const std::vector<uint> formats = makeFormats(200000);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = makeSentinelDesc();
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC before = desc;

    EXPECT_FALSE(render::fillRTVFormats(formats, desc));
    EXPECT_TRUE(descBytesEqual(before, desc));
}

TEST(fillRTVFormats_p3_idempotent_filling_twice_equals_filling_once)
{
    const std::vector<uint> formats = makeFormats(5);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC once = makeSentinelDesc();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC twice = makeSentinelDesc();

    EXPECT_TRUE(render::fillRTVFormats(formats, once));
    EXPECT_TRUE(render::fillRTVFormats(formats, twice));
    EXPECT_TRUE(render::fillRTVFormats(formats, twice));
    EXPECT_TRUE(descBytesEqual(once, twice));
}

TEST(fillRTVFormats_p1_invariant_random_format_lists_follow_the_size_limit_exactly)
{
    const auto property = [] {
        const size_t count = *rc::gen::inRange<size_t>(0, 81);
        const std::vector<uint> formats = *rc::gen::container<std::vector<uint>>(count, rc::gen::inRange<uint>(0, 200));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = makeSentinelDesc();
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC before = desc;
        const bool ok = render::fillRTVFormats(formats, desc);

        RC_ASSERT(ok == (formats.size() <= maxTargets));
        if (ok)
        {
            RC_ASSERT(static_cast<size_t>(desc.NumRenderTargets) == formats.size());
            for (size_t i = 0; i < formats.size(); ++i)
            {
                RC_ASSERT(static_cast<uint>(desc.RTVFormats[i]) == formats[i]);
            }
            for (size_t i = formats.size(); i < maxTargets; ++i)
            {
                RC_ASSERT(desc.RTVFormats[i] == before.RTVFormats[i]);
            }
        }
        else
        {
            RC_ASSERT(descBytesEqual(before, desc));
        }
    };
    EXPECT_PROPERTY("fillRTVFormats accepts exactly lists of at most eight formats and rejects without writing", property);
}

TEST(fillRTVFormats_p6_fuzz_oversized_lists_never_modify_the_desc)
{
    const auto property = [] {
        const size_t count = *rc::gen::inRange<size_t>(maxTargets + 1, 5000);
        const std::vector<uint> formats = *rc::gen::container<std::vector<uint>>(count, rc::gen::inRange<uint>(0, 200));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = makeSentinelDesc();
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC before = desc;
        RC_ASSERT(!render::fillRTVFormats(formats, desc));
        RC_ASSERT(descBytesEqual(before, desc));
    };
    EXPECT_PROPERTY("oversized format lists are rejected with the desc untouched", property);
}
