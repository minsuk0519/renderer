#include "property_harness.hpp"

#include <render/framebuffer.hpp>

TEST(framebuffer_p3_idempotent_close_twice_on_default_constructed_framebuffer)
{
    framebuffer fb;
    fb.close();
    fb.close();
}

TEST(framebuffer_p6_fuzz_repeated_close_on_default_constructed_framebuffers_never_crashes)
{
    const auto property = [] {
        const int repeats = *rc::gen::inRange<int>(1, 20);
        framebuffer fb;
        for (int i = 0; i < repeats; ++i)
        {
            fb.close();
        }
        RC_ASSERT(true);
    };
    EXPECT_PROPERTY("close is safe to call any number of times on an empty framebuffer", property);
}
