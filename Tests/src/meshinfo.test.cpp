#include "test_harness.hpp"

#include <render/buffer.hpp>
#include <system/config.hpp>
#include <system/jsonhelper.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    constexpr const char* minimalInfo =
        "Number of LODs : 1\n"
        "cluster Infos by LOD : \n"
        "0 : 2, 5\n"
        "\n"
        "(2.000000, 4.000000, 6.000000)\n"
        "3, (1.0, 2.0, 3.0), 4.0, (5.0, 6.0, 7.0), (8.0, 9.0, 10.0)\n"
        "2, (11.0, 12.0, 13.0), 14.0, (15.0, 16.0, 17.0), (18.0, 19.0, 20.0)\n";

    std::string formatFloat(float value)
    {
        char text[64];
        std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
        return text;
    }

    std::string serialize(const buf::meshInfoRecord& record)
    {
        std::string text = "Number of LODs : " + std::to_string(record.lodNum) + "\ncluster Infos by LOD : \n";
        for (uint i = 0; i < record.lodNum; ++i)
        {
            text += std::to_string(i) + " : " + std::to_string(record.lods[i].clusterCount) + ", " + std::to_string(record.lods[i].totalIndicesCount) + "\n";
        }
        text += "\n(" + formatFloat(record.halfExtent[0] * 2.0f) + ", " + formatFloat(record.halfExtent[1] * 2.0f) + ", " + formatFloat(record.halfExtent[2] * 2.0f) + ")\n";
        for (const buf::meshInfoCluster& c : record.clusters)
        {
            text += std::to_string(c.indexCount) + ", ("
                + formatFloat(c.sphereCenter[0]) + ", " + formatFloat(c.sphereCenter[1]) + ", " + formatFloat(c.sphereCenter[2]) + "), "
                + formatFloat(c.sphereRadius) + ", ("
                + formatFloat(c.aabbCenter[0]) + ", " + formatFloat(c.aabbCenter[1]) + ", " + formatFloat(c.aabbCenter[2]) + "), ("
                + formatFloat(c.aabbHalfExtent[0]) + ", " + formatFloat(c.aabbHalfExtent[1]) + ", " + formatFloat(c.aabbHalfExtent[2]) + ")\n";
        }
        return text;
    }

    std::string buildInfo(const std::vector<std::vector<uint>>& clusterIndexCountsPerLod, const std::vector<uint>& statedTotals)
    {
        std::string text = "Number of LODs : " + std::to_string(clusterIndexCountsPerLod.size()) + "\ncluster Infos by LOD : \n";
        for (std::size_t i = 0; i < clusterIndexCountsPerLod.size(); ++i)
        {
            text += std::to_string(i) + " : " + std::to_string(clusterIndexCountsPerLod[i].size()) + ", " + std::to_string(statedTotals[i]) + "\n";
        }
        text += "\n(2.0, 2.0, 2.0)\n";
        for (const std::vector<uint>& lod : clusterIndexCountsPerLod)
        {
            for (const uint indexCount : lod)
            {
                text += std::to_string(indexCount) + ", (0, 0, 0), 1.0, (0, 0, 0), (1, 1, 1)\n";
            }
        }
        return text;
    }

    std::string singleClusterInfo(const std::string& indexCountToken)
    {
        return "Number of LODs : 1\ncluster Infos by LOD : \n0 : 1, 1\n\n(2, 2, 2)\n" + indexCountToken + ", (0, 0, 0), 1.0, (0, 0, 0), (1, 1, 1)\n";
    }

    bool recordInvariantsHold(const buf::meshInfoRecord& record)
    {
        if (record.lodNum > MAX_MESHES_LOD || record.lods.size() != record.lodNum)
        {
            return false;
        }
        std::size_t clusterTotal = 0;
        std::size_t clusterCursor = 0;
        for (const buf::meshInfoLod& lod : record.lods)
        {
            clusterTotal += lod.clusterCount;
            uint indexSum = 0;
            for (uint j = 0; j < lod.clusterCount; ++j)
            {
                if (clusterCursor >= record.clusters.size())
                {
                    return false;
                }
                indexSum += record.clusters[clusterCursor].indexCount;
                ++clusterCursor;
            }
            if (indexSum != lod.totalIndicesCount)
            {
                return false;
            }
        }
        return record.clusters.size() == clusterTotal;
    }

    bool recordsEqual(const buf::meshInfoRecord& a, const buf::meshInfoRecord& b)
    {
        if (a.lodNum != b.lodNum || a.lods.size() != b.lods.size() || a.clusters.size() != b.clusters.size())
        {
            return false;
        }
        for (int k = 0; k < 3; ++k)
        {
            if (a.halfExtent[k] != b.halfExtent[k])
            {
                return false;
            }
        }
        for (std::size_t i = 0; i < a.lods.size(); ++i)
        {
            if (a.lods[i].clusterCount != b.lods[i].clusterCount || a.lods[i].totalIndicesCount != b.lods[i].totalIndicesCount)
            {
                return false;
            }
        }
        for (std::size_t i = 0; i < a.clusters.size(); ++i)
        {
            const buf::meshInfoCluster& x = a.clusters[i];
            const buf::meshInfoCluster& y = b.clusters[i];
            if (x.indexCount != y.indexCount || x.sphereRadius != y.sphereRadius)
            {
                return false;
            }
            for (int k = 0; k < 3; ++k)
            {
                if (x.sphereCenter[k] != y.sphereCenter[k] || x.aabbCenter[k] != y.aabbCenter[k] || x.aabbHalfExtent[k] != y.aabbHalfExtent[k])
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool loadRealInfo(const char* name, std::string& text)
    {
        const std::string candidates[] = {
            std::string("data/asset/model/") + name,
            std::string("Game/data/asset/model/") + name,
            std::string("../Game/data/asset/model/") + name,
        };
        for (const std::string& path : candidates)
        {
            char* data = nullptr;
            const int size = rawFileRead(path, &data);
            if (size >= 0)
            {
                text.assign(data, static_cast<std::size_t>(size));
                delete[] data;
                return true;
            }
        }
        return false;
    }

    struct lcg
    {
        std::uint32_t state;
        std::uint32_t next()
        {
            state = state * 1664525u + 1013904223u;
            return state >> 8;
        }
    };
}

TEST(parseMeshInfoText_valid_minimal_sample_matches_hand_computed_values)
{
    buf::meshInfoRecord record;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, record));

    EXPECT_EQ(record.lodNum, 1u);
    EXPECT_EQ(record.lods.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(record.clusters.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(record.halfExtent[0], 1.0f);
    EXPECT_EQ(record.halfExtent[1], 2.0f);
    EXPECT_EQ(record.halfExtent[2], 3.0f);
    if (record.lods.size() == 1 && record.clusters.size() == 2)
    {
        EXPECT_EQ(record.lods[0].clusterCount, 2u);
        EXPECT_EQ(record.lods[0].totalIndicesCount, 5u);
        EXPECT_EQ(record.clusters[0].indexCount, 3u);
        EXPECT_EQ(record.clusters[1].indexCount, 2u);
        EXPECT_EQ(record.clusters[0].sphereCenter[2], 3.0f);
        EXPECT_EQ(record.clusters[0].sphereRadius, 4.0f);
        EXPECT_EQ(record.clusters[0].aabbCenter[0], 5.0f);
        EXPECT_EQ(record.clusters[1].aabbHalfExtent[2], 20.0f);
    }
    EXPECT_TRUE(recordInvariantsHold(record));
}

TEST(parseMeshInfoText_p1_invariant_real_sphere_info_satisfies_record_invariants)
{
    std::string text;
    EXPECT_TRUE(loadRealInfo("sphere.info", text));

    buf::meshInfoRecord record;
    EXPECT_TRUE(buf::parseMeshInfoText(text, record));
    EXPECT_EQ(record.lodNum, 5u);
    EXPECT_EQ(record.clusters.size(), static_cast<std::size_t>(14 + 7 + 4 + 2 + 1));
    EXPECT_TRUE(recordInvariantsHold(record));
}

TEST(parseMeshInfoText_p1_invariant_real_bunny_info_satisfies_record_invariants)
{
    std::string text;
    EXPECT_TRUE(loadRealInfo("bunny.info", text));

    buf::meshInfoRecord record;
    EXPECT_TRUE(buf::parseMeshInfoText(text, record));
    EXPECT_TRUE(record.lodNum >= 1u);
    EXPECT_TRUE(recordInvariantsHold(record));
}

TEST(parseMeshInfoText_p4_roundtrip_real_sphere_info_serialize_then_parse_recovers_record)
{
    std::string text;
    EXPECT_TRUE(loadRealInfo("sphere.info", text));

    buf::meshInfoRecord first;
    EXPECT_TRUE(buf::parseMeshInfoText(text, first));

    buf::meshInfoRecord second;
    EXPECT_TRUE(buf::parseMeshInfoText(serialize(first), second));
    EXPECT_TRUE(recordsEqual(first, second));
}

TEST(parseMeshInfoText_p4_roundtrip_minimal_sample_serialize_then_parse_recovers_record)
{
    buf::meshInfoRecord first;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, first));

    buf::meshInfoRecord second;
    EXPECT_TRUE(buf::parseMeshInfoText(serialize(first), second));
    EXPECT_TRUE(recordsEqual(first, second));
}

TEST(parseMeshInfoText_p3_idempotent_parsing_twice_into_same_out_gives_same_record)
{
    buf::meshInfoRecord once;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, once));

    buf::meshInfoRecord twice;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, twice));
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, twice));
    EXPECT_TRUE(recordsEqual(once, twice));
}

TEST(parseMeshInfoText_p3_idempotent_dirty_out_record_is_fully_reset)
{
    buf::meshInfoRecord dirty;
    EXPECT_TRUE(buf::parseMeshInfoText(buildInfo({ { 1, 1, 1, 1, 1 } }, { 5 }), dirty));
    EXPECT_EQ(dirty.clusters.size(), static_cast<std::size_t>(5));

    buf::meshInfoRecord fresh;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, fresh));
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, dirty));
    EXPECT_TRUE(recordsEqual(fresh, dirty));
}

TEST(parseMeshInfoText_p2_metamorphic_scaling_header_extent_scales_half_extent_linearly)
{
    buf::meshInfoRecord base;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, base));

    std::string scaled = minimalInfo;
    const std::string original = "(2.000000, 4.000000, 6.000000)";
    const std::size_t at = scaled.find(original);
    EXPECT_TRUE(at != std::string::npos);
    if (at != std::string::npos)
    {
        scaled.replace(at, original.size(), "(8.000000, 16.000000, 24.000000)");
    }

    buf::meshInfoRecord bigger;
    EXPECT_TRUE(buf::parseMeshInfoText(scaled, bigger));
    for (int k = 0; k < 3; ++k)
    {
        EXPECT_EQ(bigger.halfExtent[k], base.halfExtent[k] * 4.0f);
    }
    EXPECT_EQ(bigger.clusters.size(), base.clusters.size());
}

TEST(parseMeshInfoText_p2_metamorphic_trailing_whitespace_and_crlf_do_not_change_result)
{
    buf::meshInfoRecord base;
    EXPECT_TRUE(buf::parseMeshInfoText(minimalInfo, base));

    std::string padded = std::string(minimalInfo) + "\n\n   \n";
    buf::meshInfoRecord withPadding;
    EXPECT_TRUE(buf::parseMeshInfoText(padded, withPadding));
    EXPECT_TRUE(recordsEqual(base, withPadding));

    std::string crlf;
    for (const char c : std::string(minimalInfo))
    {
        if (c == '\n')
        {
            crlf += "\r\n";
        }
        else
        {
            crlf += c;
        }
    }
    buf::meshInfoRecord withCrlf;
    const bool crlfParsed = buf::parseMeshInfoText(crlf, withCrlf);
    if (crlfParsed)
    {
        EXPECT_TRUE(recordsEqual(base, withCrlf));
    }
}

TEST(parseMeshInfoText_p6_degenerate_empty_text_fails)
{
    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText(std::string(), record));
}

TEST(parseMeshInfoText_p6_degenerate_whitespace_and_garbage_text_fail)
{
    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText("\n\n\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("   ", record));
    EXPECT_FALSE(buf::parseMeshInfoText("not a mesh info file at all", record));
    EXPECT_FALSE(buf::parseMeshInfoText(std::string(4096, 'x'), record));
    EXPECT_FALSE(buf::parseMeshInfoText(std::string("\0\0\0\0", 4), record));
}

TEST(parseMeshInfoText_p6_degenerate_key_without_value_or_newline_fails)
{
    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : ", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 1", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : \n0 : 1, 1\n\n(1,1,1)\n1, (0,0,0), 1, (0,0,0), (1,1,1)\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : abc\n0 : 1, 1\n", record));
}

TEST(parseMeshInfoText_p6_degenerate_lod_count_bounds)
{
    buf::meshInfoRecord record;

    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 0\ncluster Infos by LOD : \n\n(2,2,2)\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : -1\ncluster Infos by LOD : \n0 : 0, 0\n\n(2,2,2)\n", record));

    const std::string overCapacity = "Number of LODs : " + std::to_string(MAX_MESHES_LOD + 1) + "\ncluster Infos by LOD : \n0 : 0, 0\n\n(2,2,2)\n";
    EXPECT_FALSE(buf::parseMeshInfoText(overCapacity, record));

    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 4294967296\ncluster Infos by LOD : \n0 : 0, 0\n\n(2,2,2)\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 99999999999999999999\ncluster Infos by LOD : \n0 : 0, 0\n\n(2,2,2)\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 2147483648\ncluster Infos by LOD : \n0 : 0, 0\n\n(2,2,2)\n", record));
}

TEST(parseMeshInfoText_p1_invariant_lod_count_at_capacity_accepted_when_data_complete)
{
    std::vector<std::vector<uint>> lods(MAX_MESHES_LOD, std::vector<uint>{ 3 });
    std::vector<uint> totals(MAX_MESHES_LOD, 3);

    buf::meshInfoRecord record;
    EXPECT_TRUE(buf::parseMeshInfoText(buildInfo(lods, totals), record));
    EXPECT_EQ(record.lodNum, MAX_MESHES_LOD);
    EXPECT_TRUE(recordInvariantsHold(record));
}

TEST(parseMeshInfoText_p1_invariant_lod_count_one_over_capacity_rejected_even_with_complete_data)
{
    std::vector<std::vector<uint>> lods(MAX_MESHES_LOD + 1, std::vector<uint>{ 3 });
    std::vector<uint> totals(MAX_MESHES_LOD + 1, 3);

    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText(buildInfo(lods, totals), record));
}

TEST(parseMeshInfoText_p6_degenerate_negative_and_garbage_counts_fail)
{
    buf::meshInfoRecord record;
    const std::string tail = "\n\n(2,2,2)\n1, (0,0,0), 1, (0,0,0), (1,1,1)\n";
    const std::string head = "Number of LODs : 1\ncluster Infos by LOD : \n0 : ";

    EXPECT_FALSE(buf::parseMeshInfoText(head + "-1, 1" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "1, -1" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "x, 1" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "1, y" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "99999999999999999999, 1" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "1, 99999999999999999999" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "1 1" + tail, record));
    EXPECT_FALSE(buf::parseMeshInfoText(head + "1," + tail, record));
}

TEST(parseMeshInfoText_p6_degenerate_huge_cluster_count_without_data_fails_fast)
{
    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 1\ncluster Infos by LOD : \n0 : 2147483647, 0\n\n(2,2,2)\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 1\ncluster Infos by LOD : \n0 : 2147483647, 2147483647\n\n(2,2,2)\n1, (0,0,0), 1, (0,0,0), (1,1,1)\n", record));
}

TEST(parseMeshInfoText_p1_invariant_lod_index_must_equal_position)
{
    buf::meshInfoRecord record;
    const std::string swapped =
        "Number of LODs : 2\ncluster Infos by LOD : \n0 : 1, 1\n5 : 1, 1\n\n(2,2,2)\n"
        "1, (0,0,0), 1, (0,0,0), (1,1,1)\n1, (0,0,0), 1, (0,0,0), (1,1,1)\n";
    EXPECT_FALSE(buf::parseMeshInfoText(swapped, record));

    const std::string duplicated =
        "Number of LODs : 2\ncluster Infos by LOD : \n0 : 1, 1\n0 : 1, 1\n\n(2,2,2)\n"
        "1, (0,0,0), 1, (0,0,0), (1,1,1)\n1, (0,0,0), 1, (0,0,0), (1,1,1)\n";
    EXPECT_FALSE(buf::parseMeshInfoText(duplicated, record));
}

TEST(parseMeshInfoText_p1_invariant_per_lod_index_sum_must_equal_stated_total)
{
    buf::meshInfoRecord record;
    EXPECT_TRUE(buf::parseMeshInfoText(buildInfo({ { 3, 4 } }, { 7 }), record));
    EXPECT_FALSE(buf::parseMeshInfoText(buildInfo({ { 3, 4 } }, { 6 }), record));
    EXPECT_FALSE(buf::parseMeshInfoText(buildInfo({ { 3, 4 } }, { 8 }), record));
    EXPECT_FALSE(buf::parseMeshInfoText(buildInfo({ { 3, 4 }, { 1 } }, { 7, 2 }), record));
    EXPECT_FALSE(buf::parseMeshInfoText(buildInfo({ { 3, 4 }, { 1 } }, { 0, 1 }), record));
}

TEST(parseMeshInfoText_p6_degenerate_cluster_index_count_range_and_nan)
{
    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("-1"), record));
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("-0.5"), record));
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("nan"), record));
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("inf"), record));
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("4294967296"), record));
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("1e30"), record));
    EXPECT_FALSE(buf::parseMeshInfoText(singleClusterInfo("abc"), record));
}

TEST(parseMeshInfoText_p6_degenerate_non_finite_geometry_values_never_crash)
{
    buf::meshInfoRecord record;
    const std::string text =
        "Number of LODs : 1\ncluster Infos by LOD : \n0 : 1, 1\n\n(nan, inf, -inf)\n"
        "1, (nan, nan, nan), inf, (1e39, -1e39, 0), (1e-50, 0, 0)\n";
    const bool parsed = buf::parseMeshInfoText(text, record);
    if (parsed)
    {
        EXPECT_TRUE(recordInvariantsHold(record));
    }
}

TEST(parseMeshInfoText_p6_degenerate_truncated_cluster_data_fails)
{
    buf::meshInfoRecord record;
    const std::string text = minimalInfo;
    const std::size_t lastLineStart = text.rfind("2, (11.0");
    EXPECT_TRUE(lastLineStart != std::string::npos);
    EXPECT_FALSE(buf::parseMeshInfoText(text.substr(0, lastLineStart), record));

    const std::string missingFields =
        "Number of LODs : 1\ncluster Infos by LOD : \n0 : 1, 1\n\n(2,2,2)\n1, (0,0,0), 1, (0,0,0), (1,1)\n";
    EXPECT_FALSE(buf::parseMeshInfoText(missingFields, record));
}

TEST(parseMeshInfoText_p6_degenerate_missing_header_extent_fails)
{
    buf::meshInfoRecord record;
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 1\ncluster Infos by LOD : \n0 : 0, 0\n\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 1\ncluster Infos by LOD : \n0 : 0, 0\n\n(2, 2)\n", record));
    EXPECT_FALSE(buf::parseMeshInfoText("Number of LODs : 1\ncluster Infos by LOD : \n0 : 0, 0\n", record));
}

TEST(parseMeshInfoText_p6_fuzz_every_prefix_of_real_sphere_info_never_crashes_and_keeps_invariants)
{
    std::string text;
    EXPECT_TRUE(loadRealInfo("sphere.info", text));

    for (std::size_t length = 0; length <= text.size(); length += 7)
    {
        buf::meshInfoRecord record;
        const bool parsed = buf::parseMeshInfoText(text.substr(0, length), record);
        if (parsed)
        {
            EXPECT_TRUE(recordInvariantsHold(record));
        }
    }

    const std::size_t headerEnd = text.find("\r\n\r\n");
    EXPECT_TRUE(headerEnd != std::string::npos);
    for (std::size_t length = 0; length < headerEnd + 4 && length <= text.size(); ++length)
    {
        buf::meshInfoRecord record;
        EXPECT_FALSE(buf::parseMeshInfoText(text.substr(0, length), record));
    }
}

TEST(parseMeshInfoText_p6_fuzz_random_byte_mutations_never_crash_and_true_results_satisfy_invariants)
{
    std::string text;
    EXPECT_TRUE(loadRealInfo("sphere.info", text));
    if (text.empty())
    {
        text = minimalInfo;
    }

    const char alphabet[] = { '0', '9', '-', '+', '.', 'e', 'n', 'a', ' ', ',', '(', ')', '\n', '\r', ':', 'x', '\0' };
    lcg rng{ 12345u };

    for (int iteration = 0; iteration < 3000; ++iteration)
    {
        std::string mutated = text;
        const uint mutations = 1 + rng.next() % 6;
        for (uint m = 0; m < mutations; ++m)
        {
            const std::size_t position = rng.next() % mutated.size();
            const uint kind = rng.next() % 3;
            if (kind == 0)
            {
                mutated[position] = alphabet[rng.next() % sizeof(alphabet)];
            }
            else if (kind == 1)
            {
                mutated.erase(position, 1 + rng.next() % 4);
            }
            else
            {
                mutated.insert(position, 1, alphabet[rng.next() % sizeof(alphabet)]);
            }
            if (mutated.empty())
            {
                mutated = "x";
            }
        }
        if (rng.next() % 4 == 0)
        {
            mutated.resize(rng.next() % mutated.size());
        }

        buf::meshInfoRecord record;
        const bool parsed = buf::parseMeshInfoText(mutated, record);
        if (parsed)
        {
            EXPECT_TRUE(recordInvariantsHold(record));
        }
    }
}

TEST(parseMeshInfoText_p6_fuzz_random_ascii_noise_never_crashes)
{
    lcg rng{ 987654u };
    for (int iteration = 0; iteration < 500; ++iteration)
    {
        std::string noise = "Number of LODs : ";
        const uint length = rng.next() % 200;
        for (uint i = 0; i < length; ++i)
        {
            noise += static_cast<char>(rng.next() % 128);
        }
        buf::meshInfoRecord record;
        const bool parsed = buf::parseMeshInfoText(noise, record);
        if (parsed)
        {
            EXPECT_TRUE(recordInvariantsHold(record));
        }
    }
}
