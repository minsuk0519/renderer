#include "test_harness.hpp"

#include <system/jsonhelper.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    char* const sentinelPointer = reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x1));

    struct tempFile
    {
        std::filesystem::path path;

        explicit tempFile(const std::string& tag)
            : path(std::filesystem::temp_directory_path() / ("rawfileread_test_" + tag + ".bin"))
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        ~tempFile()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        tempFile(const tempFile&) = delete;
        tempFile& operator=(const tempFile&) = delete;

        void write(const std::vector<char>& bytes) const
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!bytes.empty())
            {
                out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            }
        }

        void writeFilled(std::size_t size, char fill) const
        {
            write(std::vector<char>(size, fill));
        }

        std::string str() const
        {
            return path.string();
        }
    };

    struct readResult
    {
        int size = 0;
        char* data = sentinelPointer;
    };

    readResult readFile(const std::string& name, uint bufferSize = 0)
    {
        readResult result;
        result.size = rawFileRead(name, &result.data, bufferSize);
        return result;
    }
}

TEST(rawFileRead_p6_degenerate_missing_file_returns_error_and_null_data)
{
    const tempFile file("missing");
    const readResult result = readFile(file.str());

    EXPECT_TRUE(result.size < 0);
    EXPECT_TRUE(result.data == nullptr);
}

TEST(rawFileRead_p6_degenerate_empty_filename_returns_error_and_null_data)
{
    const readResult result = readFile(std::string());

    EXPECT_TRUE(result.size < 0);
    EXPECT_TRUE(result.data == nullptr);
}

TEST(rawFileRead_p6_degenerate_directory_path_returns_error_and_null_data)
{
    const readResult result = readFile(std::filesystem::temp_directory_path().string());

    EXPECT_TRUE(result.size < 0);
    EXPECT_TRUE(result.data == nullptr);
}

TEST(rawFileRead_p6_degenerate_empty_file_reads_zero_bytes_with_terminator)
{
    const tempFile file("empty");
    file.write({});

    const readResult result = readFile(file.str());

    EXPECT_EQ(result.size, 0);
    EXPECT_TRUE(result.data != nullptr);
    if (result.data != nullptr)
    {
        EXPECT_EQ(result.data[0], '\0');
    }
    delete[] result.data;
}

TEST(rawFileRead_p1_invariant_returned_size_matches_file_size_and_terminator_follows)
{
    const std::vector<std::size_t> sizes = { 1, 2, 3, 7, 255, 256, 257, 4095, 4096, 4097, 65536 };
    for (const std::size_t size : sizes)
    {
        const tempFile file("sizes");
        file.writeFilled(size, 'x');

        const readResult result = readFile(file.str());

        EXPECT_EQ(result.size, static_cast<int>(size));
        EXPECT_TRUE(result.data != nullptr);
        if (result.data != nullptr)
        {
            EXPECT_EQ(result.data[size], '\0');
            EXPECT_EQ(result.data[0], 'x');
            EXPECT_EQ(result.data[size - 1], 'x');
        }
        delete[] result.data;
    }
}

TEST(rawFileRead_p4_roundtrip_binary_content_with_embedded_nul_is_preserved)
{
    std::vector<char> bytes;
    for (int i = 0; i < 1024; ++i)
    {
        bytes.push_back(static_cast<char>(i % 256));
    }
    const tempFile file("binary");
    file.write(bytes);

    const readResult result = readFile(file.str());

    EXPECT_EQ(result.size, static_cast<int>(bytes.size()));
    EXPECT_TRUE(result.data != nullptr);
    if (result.data != nullptr)
    {
        EXPECT_EQ(std::memcmp(result.data, bytes.data(), bytes.size()), 0);
        EXPECT_EQ(result.data[bytes.size()], '\0');
    }
    delete[] result.data;
}

TEST(rawFileRead_p1_invariant_small_cap_accepts_file_exactly_at_cap)
{
    const tempFile file("capexact");
    file.writeFilled(10, 'a');

    const readResult result = readFile(file.str(), 10);

    EXPECT_EQ(result.size, 10);
    EXPECT_TRUE(result.data != nullptr);
    if (result.data != nullptr)
    {
        EXPECT_EQ(result.data[10], '\0');
    }
    delete[] result.data;
}

TEST(rawFileRead_p1_invariant_small_cap_rejects_file_one_byte_over_cap_with_null_data)
{
    const tempFile file("capover");
    file.writeFilled(11, 'a');

    const readResult result = readFile(file.str(), 10);

    EXPECT_TRUE(result.size < 0);
    EXPECT_TRUE(result.data == nullptr);
}

TEST(rawFileRead_p1_invariant_small_cap_one_byte_boundaries)
{
    const tempFile fileAtCap("cap1exact");
    fileAtCap.writeFilled(1, 'a');
    const readResult atCap = readFile(fileAtCap.str(), 1);
    EXPECT_EQ(atCap.size, 1);
    delete[] atCap.data;

    const tempFile fileOverCap("cap1over");
    fileOverCap.writeFilled(2, 'a');
    const readResult overCap = readFile(fileOverCap.str(), 1);
    EXPECT_TRUE(overCap.size < 0);
    EXPECT_TRUE(overCap.data == nullptr);

    const tempFile fileEmpty("cap1empty");
    fileEmpty.write({});
    const readResult empty = readFile(fileEmpty.str(), 1);
    EXPECT_EQ(empty.size, 0);
    delete[] empty.data;
}

TEST(rawFileRead_p2_metamorphic_raising_cap_never_turns_success_into_failure)
{
    const tempFile file("monotone");
    file.writeFilled(100, 'm');

    bool seenSuccess = false;
    for (uint cap = 1; cap <= 200; ++cap)
    {
        const readResult result = readFile(file.str(), cap);
        const bool success = result.size >= 0;
        if (seenSuccess)
        {
            EXPECT_TRUE(success);
        }
        if (success)
        {
            seenSuccess = true;
            EXPECT_TRUE(cap >= 100);
            EXPECT_EQ(result.size, 100);
        }
        else
        {
            EXPECT_TRUE(result.data == nullptr);
            EXPECT_TRUE(cap < 100);
        }
        delete[] result.data;
    }
    EXPECT_TRUE(seenSuccess);
}

TEST(rawFileRead_p3_idempotent_repeated_reads_return_identical_content)
{
    const tempFile file("repeat");
    file.writeFilled(333, 'r');

    for (int i = 0; i < 20; ++i)
    {
        const readResult result = readFile(file.str());
        EXPECT_EQ(result.size, 333);
        EXPECT_TRUE(result.data != nullptr);
        if (result.data != nullptr)
        {
            EXPECT_EQ(std::string(result.data, 333), std::string(333, 'r'));
        }
        delete[] result.data;
    }
}

TEST(rawFileRead_p3_idempotent_repeated_failures_always_null_data_even_after_success)
{
    const tempFile good("goodthenbad");
    good.writeFilled(5, 'g');
    const tempFile bad("neverexists");

    const readResult first = readFile(good.str());
    EXPECT_EQ(first.size, 5);
    delete[] first.data;

    for (int i = 0; i < 5; ++i)
    {
        char* data = sentinelPointer;
        const int size = rawFileRead(bad.str(), &data);
        EXPECT_TRUE(size < 0);
        EXPECT_TRUE(data == nullptr);
    }
}

TEST(rawFileRead_p3_idempotent_data_pointer_overwritten_even_when_previously_non_null_on_failure)
{
    const tempFile good("prevptr");
    good.writeFilled(4, 'p');

    char* data = nullptr;
    const int firstSize = rawFileRead(good.str(), &data);
    EXPECT_EQ(firstSize, 4);
    delete[] data;

    data = new char[1];
    char* stale = data;
    const tempFile missing("prevptrmissing");
    const int size = rawFileRead(missing.str(), &data);
    EXPECT_TRUE(size < 0);
    EXPECT_TRUE(data == nullptr);
    delete[] stale;
}

TEST(rawFileRead_p1_invariant_default_cap_boundary_at_64_mebibytes)
{
    const tempFile file("defaultcap");
    const std::size_t cap = static_cast<std::size_t>(BUFFERSIZE);
    file.writeFilled(cap + 1, 'd');

    const readResult over = readFile(file.str());
    EXPECT_TRUE(over.size < 0);
    EXPECT_TRUE(over.data == nullptr);

    std::error_code ec;
    std::filesystem::resize_file(file.path, cap, ec);
    EXPECT_FALSE(static_cast<bool>(ec));

    const readResult exact = readFile(file.str());
    EXPECT_EQ(exact.size, static_cast<int>(cap));
    EXPECT_TRUE(exact.data != nullptr);
    if (exact.data != nullptr)
    {
        EXPECT_EQ(exact.data[cap], '\0');
        EXPECT_EQ(exact.data[cap - 1], 'd');
    }
    delete[] exact.data;
}

TEST(rawFileRead_p6_fuzz_cap_sweep_never_returns_data_on_failure_or_missing_terminator)
{
    const std::vector<std::size_t> fileSizes = { 0, 1, 2, 15, 16, 17 };
    for (const std::size_t fileSize : fileSizes)
    {
        const tempFile file("fuzzcap");
        file.writeFilled(fileSize, 'z');

        for (uint cap = 0; cap <= 20; ++cap)
        {
            const readResult result = readFile(file.str(), cap);
            if (result.size >= 0)
            {
                EXPECT_EQ(static_cast<std::size_t>(result.size), fileSize);
                EXPECT_TRUE(result.data != nullptr);
                if (result.data != nullptr)
                {
                    EXPECT_EQ(result.data[fileSize], '\0');
                }
            }
            else
            {
                EXPECT_TRUE(result.data == nullptr);
            }
            delete[] result.data;
        }
    }
}
