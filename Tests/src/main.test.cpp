#include "test_harness.hpp"

int main()
{
    int passed = 0;
    int failed = 0;

    for (const test::testCase& testCase : test::registry())
    {
        test::failureCount() = 0;

        try
        {
            testCase.run();
        }
        catch (const std::exception& exception)
        {
            test::recordFailure("<exception>", 0, exception.what());
        }
        catch (...)
        {
            test::recordFailure("<exception>", 0, "unknown exception");
        }

        if (test::failureCount() == 0)
        {
            std::printf("[PASS] %s\n", testCase.name);
            ++passed;
        }
        else
        {
            std::printf("[FAIL] %s\n", testCase.name);
            ++failed;
        }
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return (failed == 0 && passed > 0) ? 0 : 1;
}
