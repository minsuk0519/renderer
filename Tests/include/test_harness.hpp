#pragma once

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace test
{
    struct testCase
    {
        const char* name;
        void (*run)();
    };

    inline std::vector<testCase>& registry()
    {
        static std::vector<testCase> tests;
        return tests;
    }

    inline int& failureCount()
    {
        static int failures = 0;
        return failures;
    }

    inline void recordFailure(const char* file, int line, const std::string& message)
    {
        ++failureCount();
        std::printf("    %s:%d: %s\n", file, line, message.c_str());
    }

    struct registrar
    {
        registrar(const char* name, void (*run)())
        {
            registry().push_back(testCase{ name, run });
        }
    };
}

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST(name)                                                              \
    static void name();                                                         \
    static const ::test::registrar TEST_CONCAT(testRegistrar_, name)(#name, &name); \
    static void name()

#define EXPECT_TRUE(expr)                                                       \
    do {                                                                        \
        if (!(expr))                                                            \
        {                                                                       \
            ::test::recordFailure(__FILE__, __LINE__, "EXPECT_TRUE(" #expr ")"); \
        }                                                                       \
    } while (0)

#define EXPECT_FALSE(expr)                                                      \
    do {                                                                        \
        if (expr)                                                               \
        {                                                                       \
            ::test::recordFailure(__FILE__, __LINE__, "EXPECT_FALSE(" #expr ")"); \
        }                                                                       \
    } while (0)

#define EXPECT_EQ(a, b)                                                         \
    do {                                                                        \
        if (!((a) == (b)))                                                      \
        {                                                                       \
            ::test::recordFailure(__FILE__, __LINE__, "EXPECT_EQ(" #a ", " #b ")"); \
        }                                                                       \
    } while (0)

#define EXPECT_NE(a, b)                                                         \
    do {                                                                        \
        if (!((a) != (b)))                                                      \
        {                                                                       \
            ::test::recordFailure(__FILE__, __LINE__, "EXPECT_NE(" #a ", " #b ")"); \
        }                                                                       \
    } while (0)
