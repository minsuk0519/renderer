#include "property_harness.hpp"

#include <world/object.hpp>
#include <world/world.hpp>

#include <string>

TEST(worldClose_p3_idempotent_close_twice_after_objects_allocated_frees_and_nulls_everything)
{
    world w;
    w.objects = new object[MAX_OBJECTS];
    w.objectNum = 3;
    w.mapName = "map";

    w.close();
    EXPECT_TRUE(w.objects == nullptr);
    EXPECT_EQ(w.objectNum, 0u);
    EXPECT_TRUE(w.getMainCam() == nullptr);
    EXPECT_TRUE(w.mapName.empty());

    w.close();
    EXPECT_TRUE(w.objects == nullptr);
    EXPECT_EQ(w.objectNum, 0u);
    EXPECT_TRUE(w.getMainCam() == nullptr);
}

TEST(worldClose_p6_close_on_never_initialised_world_is_safe_and_repeatable)
{
    world w;
    w.close();
    w.close();
    w.close();

    EXPECT_TRUE(w.objects == nullptr);
    EXPECT_EQ(w.objectNum, 0u);
    EXPECT_TRUE(w.getMainCam() == nullptr);
}

TEST(worldClose_p6_stale_object_count_without_objects_is_reset_to_zero)
{
    world w;
    w.objectNum = MAX_OBJECTS;
    w.mapName = "stale";

    w.close();
    EXPECT_TRUE(w.objects == nullptr);
    EXPECT_EQ(w.objectNum, 0u);
    EXPECT_TRUE(w.mapName.empty());
}

TEST(worldClose_p3_idempotent_random_alloc_and_repeated_close_keeps_world_empty)
{
    const auto property = [] {
        world w;
        const bool allocate = *rc::gen::arbitrary<bool>();
        if (allocate)
        {
            w.objects = new object[MAX_OBJECTS];
        }
        w.objectNum = *rc::gen::inRange<uint>(0, MAX_OBJECTS + 1);
        w.mapName = *rc::gen::arbitrary<std::string>();

        const int repeats = *rc::gen::inRange<int>(1, 6);
        for (int i = 0; i < repeats; ++i)
        {
            w.close();
            RC_ASSERT(w.objects == nullptr);
            RC_ASSERT(w.objectNum == 0u);
            RC_ASSERT(w.mapName.empty());
            RC_ASSERT(w.getMainCam() == nullptr);
        }
    };
    EXPECT_PROPERTY("world::close always leaves an empty world no matter how often it is called", property);
}
