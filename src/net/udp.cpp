#include "net/udp.h"
#include <cstring>
#include "core/bytes.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SockLen = int;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SockLen = socklen_t;
#endif

namespace vox {

namespace {

constexpr uint8_t kFragmentByte = 0xFA;

void ensureSocketsInit() {
#ifdef _WIN32
    static bool done = [] {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        return true;
    }();
    (void)done;
#endif
}

// Join control packet: single magic byte + team (never fragmented).
constexpr uint8_t kJoinByte = 0xF7;

} // namespace

NetAddress loopbackAddress(uint16_t port) { return { 0x7F000001u, port }; }

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(uint16_t requestPort) {
    ensureSocketsInit();
    const auto s = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (s == INVALID_SOCKET) return false;
#else
    if (s < 0) return false;
#endif
    handle = static_cast<long long>(s);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(requestPort);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close();
        return false;
    }
    SockLen len = sizeof(addr);
    ::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len);
    port = ntohs(addr.sin_port);

#ifdef _WIN32
    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    return true;
}

void UdpSocket::close() {
    if (handle < 0) return;
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(handle));
#else
    ::close(static_cast<int>(handle));
#endif
    handle = -1;
}

bool UdpSocket::sendTo(const NetAddress& to, const uint8_t* data, size_t len) {
    if (handle < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(to.ip);
    addr.sin_port = htons(to.port);
    const auto sent = ::sendto(
#ifdef _WIN32
        static_cast<SOCKET>(handle), reinterpret_cast<const char*>(data), static_cast<int>(len),
#else
        static_cast<int>(handle), data, len,
#endif
        0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent >= 0 && static_cast<size_t>(sent) == len;
}

std::optional<std::pair<NetAddress, std::vector<uint8_t>>> UdpSocket::receive() {
    if (handle < 0) return std::nullopt;
    uint8_t buffer[2048];
    sockaddr_in from{};
    SockLen fromLen = sizeof(from);
    const auto n = ::recvfrom(
#ifdef _WIN32
        static_cast<SOCKET>(handle), reinterpret_cast<char*>(buffer), sizeof(buffer),
#else
        static_cast<int>(handle), buffer, sizeof(buffer),
#endif
        0, reinterpret_cast<sockaddr*>(&from), &fromLen);
    if (n <= 0) return std::nullopt;
    NetAddress addr{ ntohl(from.sin_addr.s_addr), ntohs(from.sin_port) };
    return std::make_pair(addr, std::vector<uint8_t>(buffer, buffer + n));
}

// ------------------------------------------------------------- fragments ----

void FragmentChannel::sendMessage(UdpSocket& socket, const NetAddress& to,
                                  const std::vector<uint8_t>& message) {
    const uint32_t id = nextMessageId++;
    const size_t count = std::max<size_t>(1, (message.size() + kFragmentPayload - 1)
                                                 / kFragmentPayload);
    for (size_t i = 0; i < count; ++i) {
        ByteWriter w;
        w.u8(kFragmentByte);
        w.u32(id);
        w.u32(static_cast<uint32_t>(i));
        w.u32(static_cast<uint32_t>(count));
        const size_t begin = i * kFragmentPayload;
        const size_t end = std::min(message.size(), begin + kFragmentPayload);
        w.data.insert(w.data.end(), message.begin() + static_cast<long>(begin),
                      message.begin() + static_cast<long>(end));
        socket.sendTo(to, w.data.data(), w.data.size());
    }
}

std::vector<std::vector<uint8_t>> FragmentChannel::onDatagram(
    const std::vector<uint8_t>& datagram) {
    std::vector<std::vector<uint8_t>> complete;
    ByteReader r(datagram);
    if (r.u8() != kFragmentByte) return complete;
    const uint32_t id = r.u32();
    const uint32_t index = r.u32();
    const uint32_t count = r.u32();
    if (!r.ok || count == 0 || count > 4096 || index >= count) return complete;

    Pending& p = reassembly[id];
    if (p.fragments.empty()) {
        p.fragments.resize(count);
        p.haveFragment.assign(count, false);
    }
    if (p.fragments.size() != count) {
        reassembly.erase(id);
        return complete;
    }
    if (!p.haveFragment[index]) {
        p.fragments[index].assign(datagram.begin() + static_cast<long>(r.pos), datagram.end());
        p.haveFragment[index] = true;
        ++p.received;
    }
    if (p.received == p.fragments.size()) {
        std::vector<uint8_t> message;
        for (const auto& frag : p.fragments)
            message.insert(message.end(), frag.begin(), frag.end());
        reassembly.erase(id);
        // Trim stale partial messages so the map can't grow unbounded.
        while (reassembly.size() > 64) reassembly.erase(reassembly.begin());
        complete.push_back(std::move(message));
    }
    return complete;
}

// ------------------------------------------------------------------ hosts ---

UdpServerHost::UdpServerHost(VoxelWorld world, uint64_t seed, uint16_t listenPort)
    : session(std::move(world), seed) {
    socket.open(listenPort);
}

void UdpServerHost::pumpOnce() {
    while (auto packet = socket.receive()) {
        const auto& [from, data] = *packet;
        auto it = peers.find(from);
        if (it == peers.end()) {
            // New address: only a Join control packet is accepted.
            if (data.size() == 2 && data[0] == kJoinByte) {
                const uint8_t team = data[1];
                const Int3 size = session.sim().world().size();
                const float x = 20.0f + 40.0f * static_cast<float>(peers.size());
                const float z = static_cast<float>(size.z) / 2.0f;
                const float y = static_cast<float>(
                    session.sim().world().heightAt(static_cast<int>(x), static_cast<int>(z)));
                PeerLink link;
                link.peerId = session.addClient(team, TemplateId::Brick, { x, y, z });
                peers.emplace(from, std::move(link));
            }
            continue;
        }
        for (auto& message : it->second.channel.onDatagram(data))
            session.receive(it->second.peerId, message);
    }

    session.tick();

    for (auto& [addr, link] : peers)
        for (const auto& message : session.takeOutbox(link.peerId))
            link.channel.sendMessage(socket, addr, message);
}

bool UdpClientLink::connect(const NetAddress& serverAddr, uint8_t team) {
    if (!socket.open(0)) return false;
    server = serverAddr;
    const uint8_t join[2] = { kJoinByte, team };
    return socket.sendTo(server, join, sizeof(join));
}

void UdpClientLink::pump(Client& client) {
    while (auto packet = socket.receive()) {
        for (auto& message : channel.onDatagram(packet->second)) client.receive(message);
    }
    for (const auto& message : client.takeOutbox()) send(message);
}

void UdpClientLink::send(const std::vector<uint8_t>& message) {
    channel.sendMessage(socket, server, message);
}

} // namespace vox
