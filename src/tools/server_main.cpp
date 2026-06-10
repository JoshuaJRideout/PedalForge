// voxfall_server — headless dedicated server (DESIGN.md §11.3 tier 2).
// Plain-UDP transport for LAN/dev; internet play gets GameNetworkingSockets.
//
// Usage: voxfall_server [port] [seed] [--ticks N]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include "net/udp.h"

using namespace vox;

int main(int argc, char** argv) {
    const uint16_t port = argc > 1 ? static_cast<uint16_t>(std::atoi(argv[1])) : 27600;
    const uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1337ull;
    long long maxTicks = -1;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--ticks") == 0) maxTicks = std::atoll(argv[i + 1]);

    VoxelWorld world({ 192, 96, 192 });
    ArenaParams params;
    params.seed = seed;
    generateArena(world, params);

    UdpServerHost host(std::move(world), seed, port);
    if (!host.listening()) {
        std::fprintf(stderr, "failed to bind UDP port %u\n", port);
        return 1;
    }
    std::printf("voxfall_server listening on UDP %u (seed %llu)\n", host.port(),
                static_cast<unsigned long long>(seed));

    using Clock = std::chrono::steady_clock;
    const auto tickDuration = std::chrono::microseconds(16667); // 60 Hz
    auto next = Clock::now();
    while (maxTicks < 0 || host.server().sim().tick() < static_cast<uint64_t>(maxTicks)) {
        host.pumpOnce();
        next += tickDuration;
        std::this_thread::sleep_until(next);
    }
    std::printf("served %llu ticks, exiting\n",
                static_cast<unsigned long long>(host.server().sim().tick()));
    return 0;
}
