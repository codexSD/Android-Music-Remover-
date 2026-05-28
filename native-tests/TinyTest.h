#ifndef VOCALREMOVER_TINYTEST_H
#define VOCALREMOVER_TINYTEST_H

// Minimal self-contained test framework. No external dependencies so the
// native real-time code can be unit tested on any host with a C++17 compiler.
//
//   TEST(suite, name) { CHECK(cond); CHECK_EQ(a, b); }
//
// Link multiple translation units together; tinytest::runAll() (called from a
// shared main) runs every registered test and returns a process exit code.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tinytest {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void(int&)> fn;  // increments the failure counter
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void(int&)> fn) {
        registry().push_back({suite, name, std::move(fn)});
    }
};

inline int runAll() {
    int failedTests = 0;
    int passedTests = 0;
    for (const auto& t : registry()) {
        int localFailures = 0;
        t.fn(localFailures);
        if (localFailures == 0) {
            std::printf("[ PASS ] %s.%s\n", t.suite.c_str(), t.name.c_str());
            ++passedTests;
        } else {
            std::printf("[ FAIL ] %s.%s (%d check(s) failed)\n", t.suite.c_str(),
                        t.name.c_str(), localFailures);
            ++failedTests;
        }
    }
    std::printf("\n%d passed, %d failed, %d total\n", passedTests, failedTests,
                static_cast<int>(registry().size()));
    return failedTests == 0 ? 0 : 1;
}

}  // namespace tinytest

#define TINYTEST_CONCAT_(a, b) a##b
#define TINYTEST_CONCAT(a, b) TINYTEST_CONCAT_(a, b)

#define TEST(suite, name)                                                       \
    static void TINYTEST_CONCAT(test_##suite##_##name, _impl)(int& tt_failures); \
    static ::tinytest::Registrar TINYTEST_CONCAT(reg_##suite##_##name, _r)(       \
        #suite, #name, &TINYTEST_CONCAT(test_##suite##_##name, _impl));          \
    static void TINYTEST_CONCAT(test_##suite##_##name, _impl)(int& tt_failures)

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("    CHECK failed: %s (%s:%d)\n", #cond, __FILE__,      \
                        __LINE__);                                             \
            ++tt_failures;                                                     \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        auto tt_a = (a);                                                       \
        auto tt_b = (b);                                                       \
        if (!(tt_a == tt_b)) {                                                 \
            std::printf("    CHECK_EQ failed: %s == %s (%s:%d)\n", #a, #b,      \
                        __FILE__, __LINE__);                                   \
            ++tt_failures;                                                     \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
    do {                                                                       \
        double tt_a = (a);                                                     \
        double tt_b = (b);                                                     \
        if (std::fabs(tt_a - tt_b) > (eps)) {                                  \
            std::printf("    CHECK_NEAR failed: |%s - %s| > %s (%s:%d)\n", #a,  \
                        #b, #eps, __FILE__, __LINE__);                         \
            ++tt_failures;                                                     \
        }                                                                      \
    } while (0)

#endif  // VOCALREMOVER_TINYTEST_H
