#pragma once

#include "test_harness.hpp"

#include <rapidcheck.h>
#include <rapidcheck/detail/Configuration.h>

#include <cstdint>
#include <string>

namespace test
{
    inline std::string propertyFailureMessage(const char* description)
    {
        const std::uint64_t seed = ::rc::detail::configuration().testParams.seed;
        return std::string("property failed: ") + description + " ; reproduce with RC_PARAMS=\"seed=" + std::to_string(seed) + "\"";
    }
}

#define EXPECT_PROPERTY(description, property)                                          \
    do {                                                                                \
        if (!::rc::check(description, property))                                        \
        {                                                                               \
            ::test::recordFailure(__FILE__, __LINE__, ::test::propertyFailureMessage(description)); \
        }                                                                               \
    } while (0)
