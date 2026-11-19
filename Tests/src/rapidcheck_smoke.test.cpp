#include "property_harness.hpp"

#include <algorithm>
#include <vector>

TEST(rapidcheck_smoke_reverse_twice_is_identity)
{
    EXPECT_PROPERTY("reverse twice is identity", [] {
        const std::vector<int> values = *rc::gen::arbitrary<std::vector<int>>();

        std::vector<int> reversed = values;
        std::reverse(reversed.begin(), reversed.end());
        std::reverse(reversed.begin(), reversed.end());

        RC_ASSERT(reversed == values);
    });
}
