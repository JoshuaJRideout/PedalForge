#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include "net/net.h"
#include "world/gen.h"

namespace vox {

// Plain-UDP transport for LAN/dev/dedicated servers (DESIGN.md §11.1: the
// GameNetworkingSockets/SDR path replaces this for internet play — same
// message payloads, reliable-ordered delivery provided by that library).
// This layer adds only fragmentation/reassembly; it assumes the loopback/LAN
// no-loss case and does NOT retransmit. Do not ship ranked play on it.

struct NetAddress {
    uint32_t ip = 0;   // host byte order
    uint16_t port = 0; // host byte order
    bool operator<(const NetAddress& o) const {
        return ip != o.ip ? ip < o.ip : port < o.port;
    }
    bool operator==(const NetAddress&) const = default;
};

NetAddress loopbackAddress(uint16_t port);

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open(uint16_t port); // 0 = ephemeral
    void close();
    bool valid() const { return handle >= 0; }
    uint16_t boundPort() const { return port; }

    bool sendTo(const NetAddress& to, const uint8_t* data, size_t len);
    // Non-blocking. nullopt = nothing pending.
    std::optional<std::pair<NetAddress, std::vector<uint8_t>>> receive();

private:
    long long handle = -1;
    uint16_t port = 0;
};

// Splits messages into <= kFragmentPayload-byte datagrams and reassembles.
class FragmentChannel {
public:
    static constexpr size_t kFragmentPayload = 1200;

    void sendMessage(UdpSocket& socket, const NetAddress& to,
                     const std::vector<uint8_t>& message);
    // Feed one raw datagram; returns any now-complete messages.
    std::vector<std::vector<uint8_t>> onDatagram(const std::vector<uint8_t>& datagram);

private:
    struct Pending {
        std::vector<std::vector<uint8_t>> fragments;
        std::vector<bool> haveFragment;
        size_t received = 0;
    };
    uint32_t nextMessageId = 1;
    std::map<uint32_t, Pending> reassembly;
};

// Dedicated-server host: real socket + the existing Server session.
// First datagram from a new address must be a Join control packet.
class UdpServerHost {
public:
    UdpServerHost(VoxelWorld world, uint64_t seed, uint16_t port);
    bool listening() const { return socket.valid(); }
    uint16_t port() const { return socket.boundPort(); }

    // Drain inbound, advance one tick, flush outboxes.
    void pumpOnce();

    Server& server() { return session; }

private:
    struct PeerLink {
        uint32_t peerId = 0;
        FragmentChannel channel;
    };
    UdpSocket socket;
    Server session;
    std::map<NetAddress, PeerLink> peers;
};

// Client side of the UDP link, wrapping the existing replica Client.
class UdpClientLink {
public:
    bool connect(const NetAddress& serverAddr, uint8_t team);
    // Drain inbound into the replica; flush client-initiated messages.
    void pump(Client& client);
    void send(const std::vector<uint8_t>& message);

private:
    UdpSocket socket;
    NetAddress server;
    FragmentChannel channel;
};

} // namespace vox
