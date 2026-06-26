#include "dpc/network/BackupClient.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/network/PacketCodec.hpp"
#include "dpc/network/TransferSession.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <map>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace dpc {

namespace {

class SocketFd {
public:
    explicit SocketFd(int fd = -1) : fd_(fd) {}
    SocketFd(const SocketFd&) = delete;
    SocketFd& operator=(const SocketFd&) = delete;
    ~SocketFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    int get() const {
        return fd_;
    }
    int release() {
        int out = fd_;
        fd_ = -1;
        return out;
    }
private:
    int fd_ = -1;
};

int connectTo(const std::string& host, int port) {
    addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const auto port_text = std::to_string(port);
    const int rc = ::getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result);
    if (rc != 0) {
        throw DpcError("getaddrinfo failed: " + std::string(gai_strerror(rc)));
    }
    std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> guard(result, ::freeaddrinfo);
    for (auto* ai = result; ai; ai = ai->ai_next) {
        SocketFd fd(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
        if (fd.get() < 0) {
            continue;
        }
        if (::connect(fd.get(), ai->ai_addr, ai->ai_addrlen) == 0) {
            return fd.release();
        }
    }
    throw DpcError("connect failed: " + host + ":" + port_text);
}

ByteVector textPayload(const std::string& text) {
    return ByteVector(text.begin(), text.end());
}

std::string payloadString(const Packet& packet) {
    return std::string(packet.payload.begin(), packet.payload.end());
}

void requireNotError(const Packet& packet) {
    if (packet.type == PacketType::Error) {
        throw DpcError("server error: " + payloadString(packet));
    }
}

std::map<std::uint64_t, std::string> parseStatus(const Packet& packet) {
    requireNotError(packet);
    if (packet.type != PacketType::SessionStatus) {
        throw DpcError("expected SESSION_STATUS packet");
    }
    std::istringstream in(payloadString(packet));
    std::string header;
    in >> header;
    if (header != "DPC_SESSION_STATUS_V1") {
        throw DpcError("invalid session status payload");
    }
    std::map<std::uint64_t, std::string> out;
    std::uint64_t index = 0;
    std::string sha;
    while (in >> index >> sha) {
        out[index] = sha;
    }
    return out;
}

ByteVector makePutPayload(const std::string& session_id, const PreparedChunk& chunk) {
    std::ostringstream header;
    header << "DPC_PUT_V1\n"
           << "session_hex " << ManifestStore::hexEncodeString(session_id) << "\n"
           << "relative_path_hex " << ManifestStore::hexEncodeString(chunk.relative_path) << "\n"
           << "file_size " << chunk.file_size << "\n"
           << "file_mode " << chunk.file_mode << "\n"
           << "file_sha256 " << chunk.file_sha256 << "\n"
           << "chunk_index " << chunk.chunk_index << "\n"
           << "raw_size " << chunk.raw.size() << "\n"
           << "compressed_size " << chunk.compressed.size() << "\n\n";
    const auto header_text = header.str();
    ByteVector payload(header_text.begin(), header_text.end());
    payload.insert(payload.end(), chunk.compressed.begin(), chunk.compressed.end());
    return payload;
}

}  // namespace

int BackupClient::upload(
    const std::filesystem::path& source,
    const std::string& host,
    int port,
    const std::string& session_id,
    std::size_t exit_after_chunks) {

    const auto chunks = TransferSession::prepareChunks(source);
    SocketFd fd(connectTo(host, port));
    const auto session_hash = PacketCodec::sessionHash(session_id);

    Packet hello;
    hello.type = PacketType::Hello;
    hello.session_id_hash = session_hash;
    hello.payload = textPayload("DPC_CLIENT_V1\n");
    PacketCodec::writePacket(fd.get(), hello);
    requireNotError(PacketCodec::readPacket(fd.get()));

    Packet query;
    query.type = PacketType::QuerySession;
    query.session_id_hash = session_hash;
    query.payload = textPayload("DPC_QUERY_SESSION_V1\nsession_hex " + ManifestStore::hexEncodeString(session_id) + "\n");
    PacketCodec::writePacket(fd.get(), query);
    const auto received = parseStatus(PacketCodec::readPacket(fd.get()));

    std::size_t sent = 0;
    for (const auto& chunk : chunks) {
        const auto found = received.find(chunk.global_index);
        if (found != received.end() && found->second == chunk.chunk_sha256) {
            continue;
        }
        Packet put;
        put.type = PacketType::PutChunk;
        put.session_id_hash = session_hash;
        put.chunk_index = chunk.global_index;
        put.chunk_sha256 = PacketCodec::shaArrayFromHex(chunk.chunk_sha256);
        put.payload = makePutPayload(session_id, chunk);
        PacketCodec::writePacket(fd.get(), put);
        auto ack = PacketCodec::readPacket(fd.get());
        requireNotError(ack);
        if (ack.type != PacketType::PutChunkAck) {
            throw DpcError("expected PUT_CHUNK_ACK packet");
        }
        ++sent;
        if (exit_after_chunks > 0 && sent >= exit_after_chunks) {
            std::cerr << "client exiting after requested chunk count\n";
            return 75;
        }
    }

    std::ostringstream commit_body;
    commit_body << "DPC_COMMIT_V1\n"
                << "session_hex " << ManifestStore::hexEncodeString(session_id) << "\n"
                << "total_chunks " << chunks.size() << "\n";
    Packet commit;
    commit.type = PacketType::CommitSession;
    commit.session_id_hash = session_hash;
    commit.payload = textPayload(commit_body.str());
    PacketCodec::writePacket(fd.get(), commit);
    auto commit_ack = PacketCodec::readPacket(fd.get());
    requireNotError(commit_ack);
    if (commit_ack.type != PacketType::CommitAck) {
        throw DpcError("expected COMMIT_ACK packet");
    }
    std::cout << payloadString(commit_ack);
    return 0;
}

}  // namespace dpc
