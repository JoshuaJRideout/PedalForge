#include "test_framework.h"

int main() {
    for (const testfw::Case& c : testfw::cases()) {
        std::printf("RUN  %s\n", c.name);
        c.fn();
    }
    if (testfw::failures() == 0) {
        std::printf("OK: %zu tests passed\n", testfw::cases().size());
        return 0;
    }
    std::printf("FAILED: %d check(s)\n", testfw::failures());
    return 1;
}
