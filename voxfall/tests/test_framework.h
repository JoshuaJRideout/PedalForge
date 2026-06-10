#pragma once
#include <cstdio>
#include <vector>

// Minimal zero-dependency test harness. Swap for a real framework when the
// project grows a dependency mechanism; the TEST/CHECK surface is compatible
// enough to make that mechanical.

namespace testfw {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> all;
    return all;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { cases().push_back({ name, fn }); }
};

} // namespace testfw

#define TEST(name)                                                  \
    static void test_##name();                                      \
    static testfw::Registrar reg_##name(#name, &test_##name);       \
    static void test_##name()

#define CHECK(expr)                                                             \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ++testfw::failures();                                               \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);       \
        }                                                                       \
    } while (0)
