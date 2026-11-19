#include "property_harness.hpp"

#include <render/commandqueue.hpp>
#include <render/descriptorheap.hpp>
#include <render/mesh.hpp>
#include <render/pipelinestate.hpp>
#include <render/rootsignature.hpp>
#include <render/shader.hpp>

#include <limits>
#include <map>
#include <string>
#include <vector>

TEST(closeCmdQueue_p3_idempotent_on_never_allocated_table_leaves_all_queues_null)
{
    render::closeCmdQueue();
    render::closeCmdQueue();
    render::closeCmdQueue();

    for (uint i = 0; i < render::QUEUE_MAX; ++i)
    {
        EXPECT_TRUE(render::getCmdQueue(static_cast<render::QUEUE_INDEX>(i)) == nullptr);
    }
}

TEST(commandqueue_p6_close_with_null_fence_event_is_safe_and_repeatable)
{
    commandqueue queue;
    queue.close();
    queue.close();
    queue.close();
}

TEST(cleanUpDescHeap_p3_idempotent_on_never_initialized_table_does_not_crash)
{
    render::cleanUpDescHeap();
    render::cleanUpDescHeap();
    render::cleanUpDescHeap();
}

TEST(meshCleanUp_p3_idempotent_on_never_loaded_table_leaves_all_meshes_null)
{
    msh::cleanUp();
    msh::cleanUp();
    msh::cleanUp();

    for (uint i = 0; i < msh::MESH_END; ++i)
    {
        EXPECT_TRUE(msh::getMesh(static_cast<msh::MESH_INDEX>(i)) == nullptr);
    }
}

TEST(cleanUpPSO_p3_idempotent_on_never_initialized_table_leaves_all_psos_null)
{
    render::cleanUpPSO();
    render::cleanUpPSO();
    render::cleanUpPSO();

    for (uint i = 0; i < render::PSO_END; ++i)
    {
        EXPECT_TRUE(render::getpipelinestate(static_cast<render::PSO_INDEX>(i)) == nullptr);
    }
}

TEST(rootsignature_p6_initFromShader_with_missing_shader_fails_before_touching_hlsl_positions)
{
    shaders::cleanup();

    rootsignature rootSig;
    std::map<uint, uint> hlslPos{ { 7u, 9u } };
    const std::map<uint, uint> before = hlslPos;

    EXPECT_FALSE(rootSig.initFromShader({ 0u }, hlslPos, false));
    EXPECT_TRUE(hlslPos == before);
    EXPECT_FALSE(rootSig.initFromShader({ 0u, 1u }, hlslPos, true));
    EXPECT_TRUE(hlslPos == before);
    EXPECT_TRUE(rootSig.getrootSignature() == nullptr);
}

TEST(rootsignature_p6_initFromShader_with_empty_or_out_of_range_ids_does_not_crash_and_is_repeatable)
{
    shaders::cleanup();

    rootsignature rootSig;
    std::map<uint, uint> hlslPos;

    EXPECT_FALSE(rootSig.initFromShader({ (std::numeric_limits<uint>::max)() }, hlslPos, false));
    EXPECT_FALSE(rootSig.initFromShader({ (std::numeric_limits<uint>::max)() }, hlslPos, false));
    EXPECT_TRUE(hlslPos.empty());
}

TEST(pipelinestate_p6_init_with_missing_shaders_fails_and_leaves_no_partial_state)
{
    shaders::cleanup();

    pipelinestate pso;
    EXPECT_FALSE(pso.init("missing", 0u, 1u, { 28u }, D3D12_CULL_MODE_BACK, false, true));
    EXPECT_TRUE(pso.getRootSig() == nullptr);
    EXPECT_TRUE(pso.getPSO() == nullptr);
    EXPECT_FALSE(pso.usesHlslLoc(0u));
    EXPECT_FALSE(pso.usesHlslLoc(1u));
}

TEST(pipelinestate_p6_initCS_with_missing_shader_fails_and_leaves_no_partial_state)
{
    shaders::cleanup();

    pipelinestate pso;
    EXPECT_FALSE(pso.initCS("missingCS", 0u));
    EXPECT_TRUE(pso.getRootSig() == nullptr);
    EXPECT_TRUE(pso.getPSO() == nullptr);
    EXPECT_FALSE(pso.usesHlslLoc(0u));
}

TEST(pipelinestate_p3_idempotent_failed_init_then_close_repeatedly_is_safe)
{
    shaders::cleanup();

    pipelinestate pso;
    EXPECT_FALSE(pso.init("a", 3u, 4u, {}, D3D12_CULL_MODE_NONE, true, false));
    EXPECT_FALSE(pso.init("a", 3u, 4u, {}, D3D12_CULL_MODE_NONE, true, false));
    pso.close();
    pso.close();
    EXPECT_FALSE(pso.initCS("b", 5u));
    pso.close();
    EXPECT_TRUE(pso.getRootSig() == nullptr);
}

TEST(pipelinestate_p6_fuzz_arbitrary_shader_ids_on_empty_table_always_fail_cleanly)
{
    shaders::cleanup();

    const auto property = [] {
        const uint vs = *rc::gen::arbitrary<uint>();
        const uint ps = *rc::gen::arbitrary<uint>();
        const bool wireframe = *rc::gen::arbitrary<bool>();
        const bool depth = *rc::gen::arbitrary<bool>();

        pipelinestate graphics;
        RC_ASSERT(!graphics.init("fuzz", vs, ps, {}, D3D12_CULL_MODE_NONE, wireframe, depth));
        RC_ASSERT(graphics.getRootSig() == nullptr);

        pipelinestate compute;
        RC_ASSERT(!compute.initCS("fuzzCS", vs));
        RC_ASSERT(compute.getRootSig() == nullptr);

        rootsignature rootSig;
        std::map<uint, uint> hlslPos;
        RC_ASSERT(!rootSig.initFromShader({ vs, ps }, hlslPos, wireframe));
        RC_ASSERT(hlslPos.empty());
    };
    EXPECT_PROPERTY("null-shader paths fail without partial state for any ids", property);
}
