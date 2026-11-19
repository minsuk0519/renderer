#include "property_harness.hpp"

#include <render/hzb.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
    constexpr uint uintMax = (std::numeric_limits<uint>::max)();

    struct depthImage
    {
        uint w = 0;
        uint h = 0;
        std::vector<uint> v;

        uint at(uint x, uint y) const
        {
            return v[static_cast<size_t>(y) * w + x];
        }
    };

    depthImage foldMip(const depthImage& src, uint dstW, uint dstH)
    {
        depthImage dst;
        dst.w = dstW;
        dst.h = dstH;
        dst.v.assign(static_cast<size_t>(dstW) * dstH, uintMax);

        for (uint y = 0; y < dstH; ++y)
        {
            for (uint x = 0; x < dstW; ++x)
            {
                const uint x0 = (std::min)(2 * x, src.w - 1);
                const uint x1 = (x == dstW - 1) ? src.w - 1 : (std::min)(2 * x + 1, src.w - 1);
                const uint y0 = (std::min)(2 * y, src.h - 1);
                const uint y1 = (y == dstH - 1) ? src.h - 1 : (std::min)(2 * y + 1, src.h - 1);

                uint value = uintMax;
                for (uint sy = y0; sy <= y1; ++sy)
                {
                    for (uint sx = x0; sx <= x1; ++sx)
                    {
                        value = (std::min)(value, src.at(sx, sy));
                    }
                }
                dst.v[static_cast<size_t>(y) * dstW + x] = value;
            }
        }
        return dst;
    }

    std::vector<depthImage> buildChain(const depthImage& base)
    {
        std::vector<depthImage> chain;
        chain.push_back(base);
        const uint count = hzb::mipCount(base.w, base.h);
        for (uint m = 1; m < count; ++m)
        {
            chain.push_back(foldMip(chain.back(), hzb::mipDim(base.w, m), hzb::mipDim(base.h, m)));
        }
        return chain;
    }

    uint lookup(const std::vector<depthImage>& chain, uint px, uint py, uint mip)
    {
        const depthImage& level = chain[mip];
        const uint tx = (std::min)(px >> mip, level.w - 1);
        const uint ty = (std::min)(py >> mip, level.h - 1);
        return level.at(tx, ty);
    }

    uint bruteForceCoverageMin(const depthImage& base, uint tx, uint ty, uint mip, uint levelW, uint levelH)
    {
        const uint xBegin = tx << mip;
        const uint xEnd = (tx == levelW - 1) ? base.w - 1 : ((tx + 1) << mip) - 1;
        const uint yBegin = ty << mip;
        const uint yEnd = (ty == levelH - 1) ? base.h - 1 : ((ty + 1) << mip) - 1;

        uint value = uintMax;
        for (uint y = yBegin; y <= yEnd; ++y)
        {
            for (uint x = xBegin; x <= xEnd; ++x)
            {
                value = (std::min)(value, base.at(x, y));
            }
        }
        return value;
    }

    rc::Gen<uint> sizeGen()
    {
        return rc::gen::oneOf(rc::gen::inRange<uint>(1, 70), rc::gen::element(1u, 2u, 3u, 5u, 7u, 9u, 15u, 17u, 31u, 33u, 63u, 65u));
    }

    depthImage drawImage(uint w, uint h)
    {
        const uint valueLimit = *rc::gen::element(2u, 5u, 1000u);
        depthImage image;
        image.w = w;
        image.h = h;
        image.v = *rc::gen::container<std::vector<uint>>(static_cast<size_t>(w) * h, rc::gen::inRange<uint>(0, valueLimit));
        return image;
    }

    depthImage lcgImage(uint w, uint h)
    {
        depthImage image;
        image.w = w;
        image.h = h;
        image.v.resize(static_cast<size_t>(w) * h);
        std::uint32_t state = 12345u;
        for (uint& value : image.v)
        {
            state = state * 1664525u + 1013904223u;
            value = state >> 8;
        }
        return image;
    }
}

TEST(hzb_p1_mipCount_known_values)
{
    EXPECT_EQ(hzb::mipCount(1600, 900), 11u);
    EXPECT_EQ(hzb::mipCount(1, 1), 1u);
    EXPECT_EQ(hzb::mipCount(2, 1), 2u);
    EXPECT_EQ(hzb::mipCount(1, 2), 2u);
    EXPECT_EQ(hzb::mipCount(3, 1), 2u);
    EXPECT_EQ(hzb::mipCount(4, 4), 3u);
    EXPECT_EQ(hzb::mipCount(1024, 1024), 11u);
    EXPECT_EQ(hzb::mipCount(1023, 1023), 10u);
}

TEST(hzb_p6_mipCount_degenerate_zero_dimensions_return_zero)
{
    EXPECT_EQ(hzb::mipCount(0, 0), 0u);
    EXPECT_EQ(hzb::mipCount(0, 5), 0u);
    EXPECT_EQ(hzb::mipCount(5, 0), 0u);
    EXPECT_EQ(hzb::mipCount(0, uintMax), 0u);
    EXPECT_EQ(hzb::mipCount(uintMax, 0), 0u);
}

TEST(hzb_p6_mipCount_uint_max_terminates_with_32_levels)
{
    EXPECT_EQ(hzb::mipCount(uintMax, uintMax), 32u);
    EXPECT_EQ(hzb::mipCount(uintMax, 1), 32u);
    EXPECT_EQ(hzb::mipCount(1, uintMax), 32u);
}

TEST(hzb_p6_mipDim_degenerate_inputs)
{
    EXPECT_EQ(hzb::mipDim(0, 0), 0u);
    EXPECT_EQ(hzb::mipDim(0, 5), 0u);
    EXPECT_EQ(hzb::mipDim(0, 32), 0u);
    EXPECT_EQ(hzb::mipDim(0, uintMax), 0u);
    EXPECT_EQ(hzb::mipDim(1, 0), 1u);
    EXPECT_EQ(hzb::mipDim(1, 31), 1u);
    EXPECT_EQ(hzb::mipDim(1, 32), 1u);
    EXPECT_EQ(hzb::mipDim(1, uintMax), 1u);
    EXPECT_EQ(hzb::mipDim(uintMax, 0), uintMax);
    EXPECT_EQ(hzb::mipDim(uintMax, 31), 1u);
    EXPECT_EQ(hzb::mipDim(uintMax, 32), 1u);
    EXPECT_EQ(hzb::mipDim(uintMax, 33), 1u);
    EXPECT_EQ(hzb::mipDim(uintMax, uintMax), 1u);
}

TEST(hzb_p1_mipDim_known_values_for_1600_by_900_chain)
{
    EXPECT_EQ(hzb::mipDim(1600, 0), 1600u);
    EXPECT_EQ(hzb::mipDim(1600, 1), 800u);
    EXPECT_EQ(hzb::mipDim(1600, 4), 100u);
    EXPECT_EQ(hzb::mipDim(1600, 10), 1u);
    EXPECT_EQ(hzb::mipDim(1600, 11), 1u);
    EXPECT_EQ(hzb::mipDim(900, 1), 450u);
    EXPECT_EQ(hzb::mipDim(900, 2), 225u);
    EXPECT_EQ(hzb::mipDim(900, 3), 112u);
    EXPECT_EQ(hzb::mipDim(900, 9), 1u);
    EXPECT_EQ(hzb::mipDim(900, 10), 1u);
}

TEST(hzb_p1_invariant_mipDim_is_positive_monotone_and_halves_each_level)
{
    const auto property = [] {
        const uint base = *rc::gen::inRange<uint>(1, 100000);
        const uint mip = *rc::gen::inRange<uint>(0, 31);
        RC_ASSERT(hzb::mipDim(base, mip) >= 1u);
        RC_ASSERT(hzb::mipDim(base, mip + 1) <= hzb::mipDim(base, mip));
        RC_ASSERT(hzb::mipDim(base, mip + 1) == (std::max)(1u, hzb::mipDim(base, mip) >> 1));
    };
    EXPECT_PROPERTY("mip dimensions halve with floor and never drop below one", property);
}

TEST(hzb_p5_commutativity_mipCount_is_symmetric_and_last_level_is_one_by_one)
{
    const auto property = [] {
        const uint w = *rc::gen::inRange<uint>(1, 100000);
        const uint h = *rc::gen::inRange<uint>(1, 100000);
        const uint count = hzb::mipCount(w, h);
        RC_ASSERT(count == hzb::mipCount(h, w));
        RC_ASSERT(count >= 1u);
        RC_ASSERT(hzb::mipDim(w, count - 1) == 1u || hzb::mipDim(h, count - 1) == 1u);
        RC_ASSERT(hzb::mipDim(w, count - 1) == 1u && hzb::mipDim(h, count - 1) == 1u);
        if (count >= 2)
        {
            RC_ASSERT(hzb::mipDim(w, count - 2) > 1u || hzb::mipDim(h, count - 2) > 1u);
        }
    };
    EXPECT_PROPERTY("mipCount is symmetric and the chain ends exactly at 1x1", property);
}

TEST(hzb_p2_metamorphic_mipCount_never_decreases_when_an_image_grows)
{
    const auto property = [] {
        const uint w = *rc::gen::inRange<uint>(1, 100000);
        const uint h = *rc::gen::inRange<uint>(1, 100000);
        const uint extraW = *rc::gen::inRange<uint>(0, 100000);
        const uint extraH = *rc::gen::inRange<uint>(0, 100000);
        RC_ASSERT(hzb::mipCount(w + extraW, h + extraH) >= hzb::mipCount(w, h));
    };
    EXPECT_PROPERTY("growing an image never reduces its mip count", property);
}

TEST(hzb_p1_cpu_model_1x1_image_has_single_mip_equal_to_its_depth)
{
    depthImage image;
    image.w = 1;
    image.h = 1;
    image.v = { 42u };
    const std::vector<depthImage> chain = buildChain(image);
    EXPECT_EQ(chain.size(), 1u);
    EXPECT_EQ(lookup(chain, 0, 0, 0), 42u);
}

TEST(hzb_p6_cpu_model_degenerate_strips_1xN_and_Nx1_never_cull_wrongly)
{
    const uint lengths[] = { 2u, 3u, 5u, 8u, 9u, 17u, 100u };
    for (const uint n : lengths)
    {
        for (int orientation = 0; orientation < 2; ++orientation)
        {
            const uint w = orientation == 0 ? 1u : n;
            const uint h = orientation == 0 ? n : 1u;
            const depthImage image = lcgImage(w, h);
            const std::vector<depthImage> chain = buildChain(image);
            EXPECT_EQ(static_cast<uint>(chain.size()), hzb::mipCount(w, h));
            for (uint y = 0; y < h; ++y)
            {
                for (uint x = 0; x < w; ++x)
                {
                    for (uint m = 0; m < chain.size(); ++m)
                    {
                        EXPECT_TRUE(lookup(chain, x, y, m) <= image.at(x, y));
                    }
                }
            }
        }
    }
}

TEST(hzb_p1_cpu_model_odd_chain_401x225_lookup_never_exceeds_pixel_depth_at_any_mip)
{
    const depthImage image = lcgImage(401, 225);
    const std::vector<depthImage> chain = buildChain(image);
    EXPECT_EQ(static_cast<uint>(chain.size()), hzb::mipCount(401, 225));

    bool allConservative = true;
    for (uint y = 0; y < image.h && allConservative; ++y)
    {
        for (uint x = 0; x < image.w; ++x)
        {
            const uint depth = image.at(x, y);
            for (uint m = 0; m < chain.size(); ++m)
            {
                if (lookup(chain, x, y, m) > depth)
                {
                    allConservative = false;
                    break;
                }
            }
            if (!allConservative)
            {
                break;
            }
        }
    }
    EXPECT_TRUE(allConservative);
}

TEST(hzb_p1_invariant_random_images_lookup_never_exceeds_pixel_depth_at_any_mip)
{
    const auto property = [] {
        const uint w = *sizeGen();
        const uint h = *sizeGen();
        const depthImage image = drawImage(w, h);
        const std::vector<depthImage> chain = buildChain(image);
        RC_ASSERT(chain.size() == hzb::mipCount(w, h));

        for (uint y = 0; y < h; ++y)
        {
            for (uint x = 0; x < w; ++x)
            {
                for (uint m = 0; m < chain.size(); ++m)
                {
                    RC_ASSERT(lookup(chain, x, y, m) <= image.at(x, y));
                }
            }
        }
    };
    EXPECT_PROPERTY("hzb lookup is never farther than the pixel it stands for", property);
}

TEST(hzb_p1_invariant_random_images_every_texel_equals_min_over_its_covered_pixels)
{
    const auto property = [] {
        const uint w = *sizeGen();
        const uint h = *sizeGen();
        const depthImage image = drawImage(w, h);
        const std::vector<depthImage> chain = buildChain(image);

        for (uint m = 0; m < chain.size(); ++m)
        {
            const depthImage& level = chain[m];
            RC_ASSERT(level.w == hzb::mipDim(w, m));
            RC_ASSERT(level.h == hzb::mipDim(h, m));
            for (uint ty = 0; ty < level.h; ++ty)
            {
                for (uint tx = 0; tx < level.w; ++tx)
                {
                    RC_ASSERT(level.at(tx, ty) == bruteForceCoverageMin(image, tx, ty, m, level.w, level.h));
                }
            }
        }
    };
    EXPECT_PROPERTY("each hzb texel is the minimum of every pixel it covers, last texel extends to the edge", property);
}

TEST(hzb_p1_invariant_random_images_top_mip_equals_global_minimum)
{
    const auto property = [] {
        const uint w = *sizeGen();
        const uint h = *sizeGen();
        const depthImage image = drawImage(w, h);
        const std::vector<depthImage> chain = buildChain(image);

        RC_ASSERT(chain.back().w == 1u);
        RC_ASSERT(chain.back().h == 1u);
        RC_ASSERT(chain.back().v[0] == *std::min_element(image.v.begin(), image.v.end()));
    };
    EXPECT_PROPERTY("the final 1x1 mip is the global minimum depth", property);
}

TEST(hzb_p2_metamorphic_widening_the_image_never_makes_a_covered_lookup_larger_than_its_depth)
{
    const auto property = [] {
        const uint w = *sizeGen();
        const uint h = *sizeGen();
        const uint extra = *rc::gen::inRange<uint>(1, 40);
        const depthImage narrow = drawImage(w, h);

        depthImage wide;
        wide.w = w + extra;
        wide.h = h;
        wide.v.resize(static_cast<size_t>(wide.w) * h);
        const uint valueLimit = *rc::gen::element(2u, 1000u);
        for (uint y = 0; y < h; ++y)
        {
            for (uint x = 0; x < wide.w; ++x)
            {
                wide.v[static_cast<size_t>(y) * wide.w + x] = x < w ? narrow.at(x, y) : *rc::gen::inRange<uint>(0, valueLimit);
            }
        }

        const std::vector<depthImage> wideChain = buildChain(wide);
        for (uint y = 0; y < h; ++y)
        {
            for (uint x = 0; x < w; ++x)
            {
                for (uint m = 0; m < wideChain.size(); ++m)
                {
                    RC_ASSERT(lookup(wideChain, x, y, m) <= narrow.at(x, y));
                }
            }
        }
    };
    EXPECT_PROPERTY("widening an image keeps every original pixel's lookup at or below its depth", property);
}

TEST(hzb_p2_metamorphic_lowering_one_depth_never_raises_any_mip_texel)
{
    const auto property = [] {
        const uint w = *sizeGen();
        const uint h = *sizeGen();
        const depthImage before = drawImage(w, h);
        depthImage after = before;
        const size_t index = *rc::gen::inRange<size_t>(0, before.v.size());
        after.v[index] = *rc::gen::inRange<uint>(0, before.v[index] + 1);

        const std::vector<depthImage> chainBefore = buildChain(before);
        const std::vector<depthImage> chainAfter = buildChain(after);
        RC_ASSERT(chainBefore.size() == chainAfter.size());
        for (size_t m = 0; m < chainBefore.size(); ++m)
        {
            for (size_t i = 0; i < chainBefore[m].v.size(); ++i)
            {
                RC_ASSERT(chainAfter[m].v[i] <= chainBefore[m].v[i]);
            }
        }
    };
    EXPECT_PROPERTY("hzb fold is monotone in the source depths", property);
}
