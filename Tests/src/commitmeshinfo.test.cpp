#include "property_harness.hpp"

#include <render/buffer.hpp>
#include <render/mesh.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace
{
    constexpr float sentinelRadius = 123.5f;

    buf::meshInfoRecord makeRecord(const std::vector<uint>& clustersPerLod)
    {
        buf::meshInfoRecord record;
        record.lodNum = static_cast<uint>(clustersPerLod.size());
        record.halfExtent[0] = 1.5f;
        record.halfExtent[1] = 2.5f;
        record.halfExtent[2] = 3.5f;
        uint clusterSeed = 1;
        for (const uint clusterCount : clustersPerLod)
        {
            buf::meshInfoLod lod;
            lod.clusterCount = clusterCount;
            lod.totalIndicesCount = 0;
            for (uint j = 0; j < clusterCount; ++j)
            {
                buf::meshInfoCluster cluster;
                cluster.indexCount = clusterSeed * 3;
                cluster.sphereCenter[0] = static_cast<float>(clusterSeed);
                cluster.sphereCenter[1] = static_cast<float>(clusterSeed) + 0.25f;
                cluster.sphereCenter[2] = static_cast<float>(clusterSeed) + 0.5f;
                cluster.sphereRadius = static_cast<float>(clusterSeed) * 2.0f;
                cluster.aabbCenter[0] = -static_cast<float>(clusterSeed);
                cluster.aabbCenter[1] = -static_cast<float>(clusterSeed) - 0.25f;
                cluster.aabbCenter[2] = -static_cast<float>(clusterSeed) - 0.5f;
                cluster.aabbHalfExtent[0] = static_cast<float>(clusterSeed) + 0.125f;
                cluster.aabbHalfExtent[1] = static_cast<float>(clusterSeed) + 0.375f;
                cluster.aabbHalfExtent[2] = static_cast<float>(clusterSeed) + 0.625f;
                lod.totalIndicesCount += cluster.indexCount;
                record.clusters.push_back(cluster);
                ++clusterSeed;
            }
            record.lods.push_back(lod);
        }
        return record;
    }

    meshData makeSentinel()
    {
        meshData data;
        data.lodNum = 7;
        data.lodData.assign(3, lodInfos{ 9, 99, { 1, 2, 3 } });
        data.boundData.radius = sentinelRadius;
        data.boundData.halfExtent[0] = -1.0f;
        data.boundData.halfExtent[1] = -2.0f;
        data.boundData.halfExtent[2] = -3.0f;
        data.clusterBounds.resize(5);
        return data;
    }

    bool sentinelUntouched(const meshData& data)
    {
        if (data.lodNum != 7 || data.lodData.size() != 3 || data.clusterBounds.size() != 5)
        {
            return false;
        }
        for (const lodInfos& lod : data.lodData)
        {
            if (lod.clusterNum != 9 || lod.totalIndicesCount != 99 || lod.indexSize != std::vector<uint>{ 1, 2, 3 })
            {
                return false;
            }
        }
        return data.boundData.radius == sentinelRadius && data.boundData.halfExtent[0] == -1.0f &&
               data.boundData.halfExtent[1] == -2.0f && data.boundData.halfExtent[2] == -3.0f;
    }

    bool matchesRecord(const meshData& data, const buf::meshInfoRecord& record)
    {
        if (data.lodNum != record.lodNum || data.lodData.size() != record.lodNum || data.clusterBounds.size() != record.clusters.size())
        {
            return false;
        }
        size_t clusterIndex = 0;
        for (uint i = 0; i < record.lodNum; ++i)
        {
            const lodInfos& lod = data.lodData[i];
            if (lod.clusterNum != record.lods[i].clusterCount || lod.totalIndicesCount != record.lods[i].totalIndicesCount || lod.indexSize.size() != record.lods[i].clusterCount)
            {
                return false;
            }
            for (uint j = 0; j < record.lods[i].clusterCount; ++j)
            {
                if (lod.indexSize[j] != record.clusters[clusterIndex].indexCount)
                {
                    return false;
                }
                ++clusterIndex;
            }
        }
        for (size_t c = 0; c < record.clusters.size(); ++c)
        {
            const buf::meshInfoCluster& src = record.clusters[c];
            const clusterbounddata& dst = data.clusterBounds[c];
            for (int axis = 0; axis < 3; ++axis)
            {
                if (dst.sphere.center[axis] != src.sphereCenter[axis] || dst.aabb.center[axis] != src.aabbCenter[axis] || dst.aabb.hExtent[axis] != src.aabbHalfExtent[axis])
                {
                    return false;
                }
            }
            if (dst.sphere.radius != src.sphereRadius)
            {
                return false;
            }
        }
        return data.boundData.halfExtent[0] == record.halfExtent[0] && data.boundData.halfExtent[1] == record.halfExtent[1] && data.boundData.halfExtent[2] == record.halfExtent[2];
    }

    bool sameMeshData(const meshData& a, const meshData& b)
    {
        if (a.lodNum != b.lodNum || a.lodData.size() != b.lodData.size() || a.clusterBounds.size() != b.clusterBounds.size())
        {
            return false;
        }
        for (size_t i = 0; i < a.lodData.size(); ++i)
        {
            if (a.lodData[i].clusterNum != b.lodData[i].clusterNum || a.lodData[i].totalIndicesCount != b.lodData[i].totalIndicesCount || a.lodData[i].indexSize != b.lodData[i].indexSize)
            {
                return false;
            }
        }
        for (size_t i = 0; i < a.clusterBounds.size(); ++i)
        {
            const clusterbounddata& x = a.clusterBounds[i];
            const clusterbounddata& y = b.clusterBounds[i];
            if (x.sphere.radius != y.sphere.radius)
            {
                return false;
            }
            for (int axis = 0; axis < 3; ++axis)
            {
                if (x.sphere.center[axis] != y.sphere.center[axis] || x.aabb.center[axis] != y.aabb.center[axis] || x.aabb.hExtent[axis] != y.aabb.hExtent[axis])
                {
                    return false;
                }
            }
        }
        return a.boundData.radius == b.boundData.radius && a.boundData.halfExtent[0] == b.boundData.halfExtent[0] &&
               a.boundData.halfExtent[1] == b.boundData.halfExtent[1] && a.boundData.halfExtent[2] == b.boundData.halfExtent[2];
    }
}

TEST(commitMeshInfo_p1_valid_record_is_copied_into_mesh_data_and_radius_is_untouched)
{
    const buf::meshInfoRecord record = makeRecord({ 2, 0, 3 });
    meshData out = makeSentinel();

    EXPECT_TRUE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(matchesRecord(out, record));
    EXPECT_EQ(out.boundData.radius, sentinelRadius);
}

TEST(commitMeshInfo_p6_zero_lod_num_is_rejected_without_touching_output)
{
    buf::meshInfoRecord record = makeRecord({});
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_lod_num_larger_than_lods_vector_is_rejected_without_touching_output)
{
    buf::meshInfoRecord record = makeRecord({ 1 });
    record.lodNum = 2;
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_lod_num_smaller_than_lods_vector_is_rejected_without_touching_output)
{
    buf::meshInfoRecord record = makeRecord({ 1, 1 });
    record.lodNum = 1;
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_huge_lod_num_with_empty_lods_is_rejected_without_allocating)
{
    buf::meshInfoRecord record;
    record.lodNum = (std::numeric_limits<uint>::max)();
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_cluster_count_sum_larger_than_cluster_list_is_rejected)
{
    buf::meshInfoRecord record = makeRecord({ 2, 2 });
    record.clusters.pop_back();
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_cluster_count_sum_smaller_than_cluster_list_is_rejected)
{
    buf::meshInfoRecord record = makeRecord({ 2, 2 });
    record.clusters.push_back(buf::meshInfoCluster{});
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_huge_cluster_count_with_no_clusters_is_rejected_without_overrun)
{
    buf::meshInfoRecord record;
    record.lodNum = 1;
    buf::meshInfoLod lod;
    lod.clusterCount = (std::numeric_limits<uint>::max)();
    record.lods.push_back(lod);
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_cluster_count_sum_wrapping_in_32_bits_is_rejected)
{
    buf::meshInfoRecord record;
    record.lodNum = 2;
    buf::meshInfoLod lod;
    lod.clusterCount = 0x80000000u;
    record.lods.push_back(lod);
    record.lods.push_back(lod);
    meshData out = makeSentinel();

    EXPECT_FALSE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(sentinelUntouched(out));
}

TEST(commitMeshInfo_p6_single_lod_with_zero_clusters_and_empty_cluster_list_is_accepted)
{
    const buf::meshInfoRecord record = makeRecord({ 0 });
    meshData out = makeSentinel();

    EXPECT_TRUE(buf::commitMeshInfo(record, out));
    EXPECT_TRUE(matchesRecord(out, record));
    EXPECT_EQ(out.lodData.size(), 1u);
    EXPECT_TRUE(out.clusterBounds.empty());
}

TEST(commitMeshInfo_p3_idempotent_committing_same_record_twice_equals_once)
{
    const buf::meshInfoRecord record = makeRecord({ 3, 2, 1 });

    meshData once = makeSentinel();
    EXPECT_TRUE(buf::commitMeshInfo(record, once));

    meshData twice = makeSentinel();
    EXPECT_TRUE(buf::commitMeshInfo(record, twice));
    EXPECT_TRUE(buf::commitMeshInfo(record, twice));

    EXPECT_TRUE(sameMeshData(once, twice));
    EXPECT_TRUE(matchesRecord(twice, record));
}

TEST(commitMeshInfo_p3_idempotent_commit_over_larger_previous_content_leaves_no_stale_data)
{
    const buf::meshInfoRecord large = makeRecord({ 4, 4, 4, 4 });
    const buf::meshInfoRecord smallRecord = makeRecord({ 1 });
    meshData out = makeSentinel();

    EXPECT_TRUE(buf::commitMeshInfo(large, out));
    EXPECT_TRUE(buf::commitMeshInfo(smallRecord, out));

    EXPECT_TRUE(matchesRecord(out, smallRecord));
    EXPECT_EQ(out.lodData.size(), 1u);
    EXPECT_EQ(out.clusterBounds.size(), 1u);
}

TEST(commitMeshInfo_p3_rejected_commit_after_accepted_commit_keeps_previous_result)
{
    const buf::meshInfoRecord good = makeRecord({ 2, 3 });
    buf::meshInfoRecord bad = makeRecord({ 5 });
    bad.clusters.clear();

    meshData out = makeSentinel();
    EXPECT_TRUE(buf::commitMeshInfo(good, out));
    meshData snapshotAfterGood = out;

    EXPECT_FALSE(buf::commitMeshInfo(bad, out));
    EXPECT_TRUE(sameMeshData(out, snapshotAfterGood));
}

TEST(commitMeshInfo_p1_invariant_random_consistent_records_are_accepted_and_copied_exactly)
{
    const auto property = [] {
        const int lodCount = *rc::gen::inRange<int>(1, 8);
        std::vector<uint> perLod;
        for (int i = 0; i < lodCount; ++i)
        {
            perLod.push_back(*rc::gen::inRange<uint>(0, 12));
        }
        const buf::meshInfoRecord record = makeRecord(perLod);
        meshData out = makeSentinel();

        RC_ASSERT(buf::commitMeshInfo(record, out));
        RC_ASSERT(matchesRecord(out, record));
        RC_ASSERT(out.boundData.radius == sentinelRadius);

        meshData again = out;
        RC_ASSERT(buf::commitMeshInfo(record, again));
        RC_ASSERT(sameMeshData(out, again));
    };
    EXPECT_PROPERTY("consistent records are accepted, copied exactly, and idempotent", property);
}

TEST(commitMeshInfo_p6_fuzz_inconsistent_records_are_rejected_and_output_untouched)
{
    const auto property = [] {
        const int lodCount = *rc::gen::inRange<int>(1, 6);
        std::vector<uint> perLod;
        for (int i = 0; i < lodCount; ++i)
        {
            perLod.push_back(*rc::gen::inRange<uint>(0, 8));
        }
        buf::meshInfoRecord record = makeRecord(perLod);

        const int mutation = *rc::gen::inRange<int>(0, 4);
        if (mutation == 0)
        {
            record.lodNum = *rc::gen::suchThat(rc::gen::arbitrary<uint>(), [lodCount](uint v) { return v != static_cast<uint>(lodCount); });
        }
        else if (mutation == 1)
        {
            record.clusters.push_back(buf::meshInfoCluster{});
        }
        else if (mutation == 2)
        {
            RC_PRE(!record.clusters.empty());
            record.clusters.pop_back();
        }
        else
        {
            const size_t target = *rc::gen::inRange<size_t>(0, record.lods.size());
            record.lods[target].clusterCount += *rc::gen::inRange<uint>(1, 1000);
        }

        meshData out = makeSentinel();
        RC_ASSERT(!buf::commitMeshInfo(record, out));
        RC_ASSERT(sentinelUntouched(out));
    };
    EXPECT_PROPERTY("inconsistent records are rejected without writing", property);
}
