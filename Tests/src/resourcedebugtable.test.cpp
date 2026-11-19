#include "property_harness.hpp"

#include <render/buffer.hpp>
#include <system/config.hpp>

#include <limits>
#include <vector>

#if ENGINE_DEBUG_RESOURCEVIEW

TEST(resourceDebugTable_p1_ensure_assigns_dense_ids_from_zero)
{
    buf::resource_debug_table table;
    EXPECT_EQ(table.count(), 0u);

    for (uint id = 0; id < 100; ++id)
    {
        buf::resourceDebugInfo* info = table.ensure(id);
        EXPECT_TRUE(info != nullptr);
        EXPECT_EQ(table.count(), id + 1);
        EXPECT_TRUE(table.find(id) == info);
    }
}

TEST(resourceDebugTable_p6_ensure_rejects_gap_on_empty_table_without_growing)
{
    buf::resource_debug_table table;
    EXPECT_TRUE(table.ensure(1) == nullptr);
    EXPECT_TRUE(table.ensure(2) == nullptr);
    EXPECT_TRUE(table.ensure(1000) == nullptr);
    EXPECT_EQ(table.count(), 0u);
}

TEST(resourceDebugTable_p6_ensure_rejects_invalid_id_and_count_plus_one_without_growing)
{
    buf::resource_debug_table table;
    EXPECT_TRUE(table.ensure(0) != nullptr);
    EXPECT_TRUE(table.ensure(1) != nullptr);
    EXPECT_EQ(table.count(), 2u);

    EXPECT_TRUE(table.ensure(BUFFER_ID_INVALID) == nullptr);
    EXPECT_TRUE(table.ensure(table.count() + 1) == nullptr);
    EXPECT_TRUE(table.ensure(table.count() + 2) == nullptr);
    EXPECT_TRUE(table.ensure((std::numeric_limits<uint>::max)() - 1) == nullptr);
    EXPECT_EQ(table.count(), 2u);
}

TEST(resourceDebugTable_p3_idempotent_ensure_of_existing_id_returns_same_entry_and_keeps_data)
{
    buf::resource_debug_table table;
    buf::resourceDebugInfo* first = table.ensure(0);
    EXPECT_TRUE(first != nullptr);
    first->width = 4242;
    first->valid = true;

    for (int i = 0; i < 10; ++i)
    {
        buf::resourceDebugInfo* again = table.ensure(0);
        EXPECT_TRUE(again == first);
        EXPECT_EQ(again->width, 4242u);
        EXPECT_TRUE(again->valid);
    }
    EXPECT_EQ(table.count(), 1u);
}

TEST(resourceDebugTable_p6_find_out_of_range_returns_null_for_boundary_ids)
{
    buf::resource_debug_table table;
    EXPECT_TRUE(table.find(0) == nullptr);
    EXPECT_TRUE(table.find(BUFFER_ID_INVALID) == nullptr);

    EXPECT_TRUE(table.ensure(0) != nullptr);
    EXPECT_TRUE(table.ensure(1) != nullptr);

    const buf::resource_debug_table& constTable = table;
    const uint ids[] = { 2u, 3u, 16384u, 65536u, (std::numeric_limits<uint>::max)() - 1u, BUFFER_ID_INVALID };
    for (const uint id : ids)
    {
        EXPECT_TRUE(table.find(id) == nullptr);
        EXPECT_TRUE(constTable.find(id) == nullptr);
    }
    EXPECT_TRUE(constTable.find(0) != nullptr);
    EXPECT_TRUE(constTable.find(1) != nullptr);
}

TEST(resourceDebugTable_p1_pointer_stability_of_earlier_entries_after_many_ensures)
{
    buf::resource_debug_table table;
    std::vector<buf::resourceDebugInfo*> pointers;

    const uint total = 20000;
    for (uint id = 0; id < total; ++id)
    {
        buf::resourceDebugInfo* info = table.ensure(id);
        EXPECT_TRUE(info != nullptr);
        if (info == nullptr)
        {
            return;
        }
        info->width = id;
        info->cpuArenaBytes = id * 2;
        pointers.push_back(info);
    }

    EXPECT_EQ(table.count(), total);
    bool stable = true;
    for (uint id = 0; id < total; ++id)
    {
        if (table.find(id) != pointers[id] || pointers[id]->width != id || pointers[id]->cpuArenaBytes != id * 2)
        {
            stable = false;
            break;
        }
    }
    EXPECT_TRUE(stable);
}

TEST(resourceDebugTable_p1_ids_at_and_beyond_old_16384_limit_work)
{
    buf::resource_debug_table table;
    for (uint id = 0; id <= 16384; ++id)
    {
        EXPECT_TRUE(table.ensure(id) != nullptr);
    }
    EXPECT_EQ(table.count(), 16385u);
    EXPECT_TRUE(table.find(16384) != nullptr);
    EXPECT_TRUE(table.ensure(16385) != nullptr);
    EXPECT_TRUE(table.find(16385) != nullptr);
    EXPECT_TRUE(table.ensure(16387) == nullptr);
}

TEST(resourceDebugTable_p1_invariant_random_ensure_sequences_match_a_dense_size_model)
{
    const auto property = [] {
        buf::resource_debug_table table;
        std::vector<buf::resourceDebugInfo*> pointers;

        const int steps = *rc::gen::inRange<int>(0, 300);
        for (int step = 0; step < steps; ++step)
        {
            const uint size = static_cast<uint>(pointers.size());
            const uint id = *rc::gen::oneOf(rc::gen::inRange<uint>(0, size + 4), rc::gen::just(BUFFER_ID_INVALID), rc::gen::just(size), rc::gen::arbitrary<uint>());
            buf::resourceDebugInfo* result = table.ensure(id);

            if (id < size)
            {
                RC_ASSERT(result == pointers[id]);
            }
            else if (id == size && id != BUFFER_ID_INVALID)
            {
                RC_ASSERT(result != nullptr);
                pointers.push_back(result);
            }
            else
            {
                RC_ASSERT(result == nullptr);
            }

            RC_ASSERT(table.count() == pointers.size());
            RC_ASSERT(table.find(static_cast<uint>(pointers.size())) == nullptr);
            for (uint i = 0; i < pointers.size(); ++i)
            {
                RC_ASSERT(table.find(i) == pointers[i]);
            }
        }
    };
    EXPECT_PROPERTY("resource_debug_table stays dense with stable entry addresses under arbitrary ensure calls", property);
}

#endif
