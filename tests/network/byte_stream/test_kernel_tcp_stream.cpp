#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include "byte_stream.hpp"
#include "kernel_tcp_stream.hpp"

using namespace qp::network::byte_stream;
using Stream = KernelTcpStream<4096, 4, 4096>;

static_assert(ByteStream<Stream>);

namespace {

constexpr auto kLocal = "127.0.0.1";

// A loopback listener on a free port.
class Listener {
   public:
    // A nonzero rcvbuf shrinks what accepted peers buffer before the sender backs up.
    explicit Listener(int rcvbuf = 0) {
        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (rcvbuf > 0)
            EXPECT_EQ(::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf), 0);
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t len        = sizeof addr;
        EXPECT_EQ(::bind(fd_, reinterpret_cast<sockaddr*>(&addr), len), 0);
        EXPECT_EQ(::listen(fd_, 1), 0);
        EXPECT_EQ(::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len), 0);
        port_ = ntohs(addr.sin_port);
    }

    ~Listener() {
        if (fd_ >= 0) ::close(fd_);
    }

    std::uint16_t port() const { return port_; }

    int accept() { return ::accept(fd_, nullptr, nullptr); }

   private:
    int           fd_ = -1;
    std::uint16_t port_{};
};

// The accepted end of a connection.
struct Peer {
    int fd;

    ~Peer() {
        if (fd >= 0) ::close(fd);
    }

    void send(std::span<const std::byte> bytes) const {
        ASSERT_EQ(::send(fd, bytes.data(), bytes.size(), 0), static_cast<ssize_t>(bytes.size()));
    }

    // Receives exactly n bytes, or fewer if a second passes.
    std::vector<std::byte> recv(std::size_t n) const {
        const timeval timeout{1, 0};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
        std::vector<std::byte> out(n);
        std::size_t            got = 0;
        while (got < n) {
            const ssize_t r = ::recv(fd, out.data() + got, n - got, 0);
            if (r <= 0) break;
            got += static_cast<std::size_t>(r);
        }
        out.resize(got);
        return out;
    }

    void close() { ::close(std::exchange(fd, -1)); }
};

std::vector<std::byte> pattern(std::size_t n, std::size_t from = 0) {
    std::vector<std::byte> out(n);
    for (std::size_t i = 0; i < n; ++i) out[i] = std::byte((from + i) % 251);
    return out;
}

// Reads until done(r) holds or a second passes.
template <class Done>
Readable read_until(Stream& s, std::size_t n, Done done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    for (;;) {
        Readable r = s.read(n);
        if (done(r) || std::chrono::steady_clock::now() > deadline) return r;
    }
}

Readable read_some(Stream& s, std::size_t n) {
    return read_until(s, n, [](const Readable& r) { return !r.lent.bytes().empty(); });
}

Written write_bytes(Stream& s, std::span<const std::byte> bytes) {
    return s.write(bytes.size(), [&](std::span<std::byte> space) {
        std::memcpy(space.data(), bytes.data(), bytes.size());
        return bytes.size();
    });
}

// Writes the pattern from pos onward in 1000 byte writes until one comes back Full.
void write_until_full(Stream& s, std::size_t& pos) {
    for (;;) {
        const Written w = write_bytes(s, pattern(1000, pos));
        if (w.status == StreamStatus::Full) return;
        ASSERT_EQ(w.status, StreamStatus::Open);
        ASSERT_EQ(w.bytes, 1000u);
        pos += w.bytes;
        ASSERT_LT(pos, std::size_t{64} << 20) << "never backed up";
    }
}

}  // namespace

TEST(KernelTcpStream, BadAddressIsError) {
    Stream s;
    EXPECT_EQ(s.connect("not an ip", 1), StreamStatus::Error);
    EXPECT_EQ(s.read(1).status, StreamStatus::Error);
}

TEST(KernelTcpStream, RefusedIsError) {
    std::uint16_t port = 0;
    {
        Listener l;
        port = l.port();
    }
    Stream s;
    EXPECT_EQ(s.connect(kLocal, port), StreamStatus::Error);
}

TEST(KernelTcpStream, ReadsNothingBeforeData) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer           p{l.accept()};
    const Readable r = s.read(64);
    EXPECT_TRUE(r.lent.bytes().empty());
    EXPECT_EQ(r.status, StreamStatus::Open);
}

TEST(KernelTcpStream, LendsAtMostN) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};
    p.send(pattern(100));

    const Readable a = read_some(s, 10);
    ASSERT_EQ(a.lent.bytes().size(), 10u);
    EXPECT_EQ(a.lent.bytes()[0], std::byte(0));
    const Readable b = read_some(s, 1000);
    EXPECT_EQ(b.lent.bytes()[0], std::byte(10));
}

TEST(KernelTcpStream, InOrderAcrossWrapsWithRisingRxTime) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};

    constexpr std::size_t kTotal = 3 * 4096 + 123;
    constexpr std::size_t kChunk = 1000;
    std::size_t           sent = 0, got = 0;
    RxTime                last = 0;
    Readable              held;  // kept across the next read
    while (got < kTotal) {
        if (sent == got) {
            const std::size_t n = std::min(kChunk, kTotal - sent);
            p.send(pattern(n, sent));
            sent += n;
        }
        Readable r = read_some(s, 700);
        ASSERT_FALSE(r.lent.bytes().empty());
        EXPECT_GT(r.rx_ts, 0);
        EXPECT_GE(r.rx_ts, last);
        last = r.rx_ts;
        for (const std::byte b : r.lent.bytes()) ASSERT_EQ(b, std::byte(got++ % 251));
        held = std::move(r);
    }
}

TEST(KernelTcpStream, PeerCloseDeliversBytesThenClosed) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};
    p.send(pattern(50));
    p.close();

    const Readable bytes = read_some(s, 1000);
    EXPECT_EQ(bytes.lent.bytes().size(), 50u);
    EXPECT_EQ(bytes.status, StreamStatus::Open);
    const Readable end =
        read_until(s, 1000, [](const Readable& r) { return r.status != StreamStatus::Open; });
    EXPECT_EQ(end.status, StreamStatus::Closed);
    EXPECT_TRUE(end.lent.bytes().empty());
    EXPECT_EQ(s.read(1).status, StreamStatus::Closed);
}

TEST(KernelTcpStream, CloseWithoutConnectIsHarmless) {
    Stream s;
    s.close();
    EXPECT_EQ(s.read(1).status, StreamStatus::Closed);
}

TEST(KernelTcpStream, CloseEndsTheConnection) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};
    s.close();

    std::byte b{};
    EXPECT_EQ(::recv(p.fd, &b, 1, 0), 0);
    EXPECT_EQ(s.read(1).status, StreamStatus::Closed);
}

TEST(KernelTcpStream, ReconnectsAfterPeerClose) {
    Listener l1, l2;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l1.port()), StreamStatus::Open);
    Peer p1{l1.accept()};
    p1.close();
    ASSERT_EQ(
        read_until(s, 1, [](const Readable& r) { return r.status != StreamStatus::Open; }).status,
        StreamStatus::Closed);

    ASSERT_EQ(s.connect(kLocal, l2.port()), StreamStatus::Open);
    Peer p2{l2.accept()};
    p2.send(pattern(20));
    const Readable r = read_some(s, 100);
    EXPECT_EQ(r.lent.bytes().size(), 20u);
    EXPECT_EQ(r.status, StreamStatus::Open);
}

TEST(KernelTcpStream, ReconnectWhileOpenClosesTheOld) {
    Listener l1, l2;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l1.port()), StreamStatus::Open);
    Peer p1{l1.accept()};
    ASSERT_EQ(s.connect(kLocal, l2.port()), StreamStatus::Open);
    Peer p2{l2.accept()};

    std::byte b{};
    EXPECT_EQ(::recv(p1.fd, &b, 1, 0), 0);
}

TEST(KernelTcpStream, ReconnectDropsUnreadBytesAndKeepsLentOnes) {
    Listener l1, l2;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l1.port()), StreamStatus::Open);
    Peer p1{l1.accept()};
    p1.send(pattern(100));
    const Readable held = read_some(s, 10);
    ASSERT_EQ(held.lent.bytes().size(), 10u);

    ASSERT_EQ(s.connect(kLocal, l2.port()), StreamStatus::Open);
    Peer p2{l2.accept()};
    p2.send(pattern(5, 200));
    const Readable r = read_some(s, 100);
    ASSERT_EQ(r.lent.bytes().size(), 5u);
    EXPECT_EQ(r.lent.bytes()[0], std::byte(200));
    for (std::size_t i = 0; i < 10; ++i) EXPECT_EQ(held.lent.bytes()[i], std::byte(i));
}

TEST(KernelTcpStream, WritesArriveInOrder) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};

    for (std::size_t i = 0; i < 50; ++i) {
        const Written w = write_bytes(s, pattern(100, i * 100));
        ASSERT_EQ(w.bytes, 100u);
        ASSERT_EQ(w.status, StreamStatus::Open);
    }
    EXPECT_EQ(p.recv(5000), pattern(5000));
}

TEST(KernelTcpStream, EchoesOverOneStream) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};

    ASSERT_EQ(write_bytes(s, pattern(30)).bytes, 30u);
    const auto echo = p.recv(30);
    ASSERT_EQ(echo, pattern(30));
    p.send(echo);
    const Readable r = read_some(s, 100);
    EXPECT_TRUE(std::ranges::equal(r.lent.bytes(), pattern(30)));
}

TEST(KernelTcpStream, WriteToClosedPeerIsErrorNotSignal) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};
    p.close();

    // The first send after the close is accepted, the reset it draws fails a later one.
    const auto   deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    StreamStatus status   = StreamStatus::Open;
    while (status == StreamStatus::Open && std::chrono::steady_clock::now() < deadline)
        status = write_bytes(s, pattern(10)).status;
    EXPECT_EQ(status, StreamStatus::Error);
    EXPECT_EQ(s.flush().status, StreamStatus::Error);
}

TEST(KernelTcpStream, BackPressureFillsThenFlushDrainsInOrder) {
    Listener l{4096};
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer p{l.accept()};

    std::size_t written = 0;
    write_until_full(s, written);
    bool          filled = false;
    const Written full   = s.write(4096, [&](std::span<std::byte>) {
        filled = true;
        return std::size_t{0};
    });
    EXPECT_EQ(full.status, StreamStatus::Full);
    EXPECT_FALSE(filled);
    EXPECT_GT(s.flush().pending, 0u);

    // The peer reads while the stream flushes, until every accepted byte has arrived.
    std::vector<std::byte> got;
    const auto             deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (got.size() < written && std::chrono::steady_clock::now() < deadline) {
        std::byte     buf[4096];
        const ssize_t r = ::recv(p.fd, buf, sizeof buf, MSG_DONTWAIT);
        if (r > 0) got.insert(got.end(), buf, buf + r);
        ASSERT_NE(s.flush().status, StreamStatus::Error);
    }
    EXPECT_EQ(got, pattern(written));
    const Flushed f = s.flush();
    EXPECT_EQ(f.pending, 0u);
    EXPECT_EQ(f.status, StreamStatus::Open);
}

TEST(KernelTcpStream, WriteWhenNotConnectedIsClosed) {
    Stream        s;
    const Written w = write_bytes(s, pattern(10));
    EXPECT_EQ(w.bytes, 0u);
    EXPECT_EQ(w.status, StreamStatus::Closed);
    const Flushed f = s.flush();
    EXPECT_EQ(f.sent, 0u);
    EXPECT_EQ(f.pending, 0u);
    EXPECT_EQ(f.status, StreamStatus::Closed);
}

TEST(KernelTcpStream, WriteLargerThanTxSizeIsError) {
    Listener l;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l.port()), StreamStatus::Open);
    Peer          p{l.accept()};
    bool          filled = false;
    const Written w      = s.write(4097, [&](std::span<std::byte>) {
        filled = true;
        return std::size_t{0};
    });
    EXPECT_EQ(w.status, StreamStatus::Error);
    EXPECT_FALSE(filled);
}

TEST(KernelTcpStream, CloseDropsUnsentBytes) {
    Listener l1{4096}, l2;
    Stream   s;
    ASSERT_EQ(s.connect(kLocal, l1.port()), StreamStatus::Open);
    Peer        p1{l1.accept()};
    std::size_t written = 0;
    write_until_full(s, written);
    ASSERT_GT(s.flush().pending, 0u);

    s.close();
    EXPECT_EQ(s.flush().pending, 0u);

    ASSERT_EQ(s.connect(kLocal, l2.port()), StreamStatus::Open);
    Peer p2{l2.accept()};
    ASSERT_EQ(write_bytes(s, pattern(5, 200)).bytes, 5u);
    EXPECT_EQ(p2.recv(5), pattern(5, 200));
    EXPECT_EQ(s.flush().pending, 0u);
}
