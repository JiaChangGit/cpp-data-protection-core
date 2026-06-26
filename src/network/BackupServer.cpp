#include "dpc/network/BackupServer.hpp"

#include "dpc/common/Error.hpp"
#include "dpc/common/FileUtils.hpp"
#include "dpc/common/Hash.hpp"
#include "dpc/core/Compressor.hpp"
#include "dpc/core/ManifestStore.hpp"
#include "dpc/core/ObjectStore.hpp"
#include "dpc/metadata/Checkpoint.hpp"
#include "dpc/metadata/CommitMarker.hpp"
#include "dpc/metadata/WalLog.hpp"
#include "dpc/network/SessionIndex.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <map>
#include <netinet/in.h>
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

ByteVector textPayload(const std::string& text) {
    return ByteVector(text.begin(), text.end());
}

std::string payloadString(const Packet& packet) {
    return std::string(packet.payload.begin(), packet.payload.end());
}

void sendText(int fd, PacketType type, std::uint64_t session_hash, const std::string& text) {
    Packet packet;
    packet.type = type;
    packet.session_id_hash = session_hash;
    packet.payload = textPayload(text);
    PacketCodec::writePacket(fd, packet);
}

void sendError(int fd, std::uint64_t session_hash, const std::string& message) {
    try {
        sendText(fd, PacketType::Error, session_hash, message + "\n");
    } catch (...) {
    }
}

std::map<std::string, std::string> parseTextFields(const std::string& text, const std::string& expected_header) {
    std::istringstream in(text);
    std::string header;
    in >> header;
    if (header != expected_header) {
        throw DpcError("invalid payload header: " + header);
    }
    std::map<std::string, std::string> fields;
    std::string key;
    std::string value;
    while (in >> key >> value) {
        fields[key] = value;
    }
    return fields;
}

std::uint64_t parseU64(const std::map<std::string, std::string>& fields, const std::string& key) {
    const auto found = fields.find(key);
    if (found == fields.end()) {
        throw DpcError("missing payload field: " + key);
    }
    return static_cast<std::uint64_t>(std::stoull(found->second));
}

std::string field(const std::map<std::string, std::string>& fields, const std::string& key) {
    const auto found = fields.find(key);
    if (found == fields.end()) {
        throw DpcError("missing payload field: " + key);
    }
    return found->second;
}

struct PutPayload {
    std::string session_id;
    std::string relative_path;
    std::uint64_t file_size = 0;
    std::uint32_t file_mode = 0644;
    std::string file_sha256;
    std::uint64_t chunk_index = 0;
    std::uint64_t raw_size = 0;
    std::uint64_t compressed_size = 0;
    ByteVector compressed;
};

PutPayload parsePutPayload(const Packet& packet) {
    const ByteVector marker {'\n', '\n'};
    auto it = std::search(packet.payload.begin(), packet.payload.end(), marker.begin(), marker.end());
    if (it == packet.payload.end()) {
        throw DpcError("PUT_CHUNK payload missing metadata separator");
    }
    std::string header(packet.payload.begin(), it);
    auto fields = parseTextFields(header, "DPC_PUT_V1");
    PutPayload out;
    out.session_id = ManifestStore::hexDecodeString(field(fields, "session_hex"));
    out.relative_path = ManifestStore::hexDecodeString(field(fields, "relative_path_hex"));
    out.file_size = parseU64(fields, "file_size");
    out.file_mode = static_cast<std::uint32_t>(parseU64(fields, "file_mode"));
    out.file_sha256 = field(fields, "file_sha256");
    out.chunk_index = parseU64(fields, "chunk_index");
    out.raw_size = parseU64(fields, "raw_size");
    out.compressed_size = parseU64(fields, "compressed_size");
    out.compressed.assign(it + 2, packet.payload.end());
    if (out.compressed.size() != out.compressed_size) {
        throw DpcError("PUT_CHUNK compressed size mismatch");
    }
    return out;
}

void writeCheckpoint(const std::filesystem::path& repo) {
    ManifestStore manifests(repo);
    auto versions = manifests.listCommittedVersions();
    CheckpointData checkpoint;
    checkpoint.committed_versions = versions;
    checkpoint.latest_committed_version = versions.empty() ? 0 : versions.back();
    Checkpoint(repo).write(checkpoint);
}

}  // namespace

BackupServer::BackupServer(std::filesystem::path repo, int port, std::size_t workers)
    : repo_(std::move(repo)), port_(port), pool_(workers == 0 ? 1 : workers, 128) {}

void BackupServer::run() {
    ObjectStore(repo_).ensureLayout();

    SocketFd server(::socket(AF_INET, SOCK_STREAM, 0));
    if (server.get() < 0) {
        throw DpcError(fileutil::errnoMessage("socket failed"));
    }
    int yes = 1;
    if (::setsockopt(server.get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        throw DpcError(fileutil::errnoMessage("setsockopt SO_REUSEADDR failed"));
    }
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(port_));
    if (::bind(server.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw DpcError(fileutil::errnoMessage("bind failed"));
    }
    if (::listen(server.get(), 64) < 0) {
        throw DpcError(fileutil::errnoMessage("listen failed"));
    }
    std::cout << "backup-server listening on port " << port_ << "\n";

    while (!stopped_.load()) {
        int client = ::accept(server.get(), nullptr, nullptr);
        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw DpcError(fileutil::errnoMessage("accept failed"));
        }
        if (!pool_.submit([this, client] { handleConnection(client); })) {
            ::close(client);
        }
    }
}

void BackupServer::stop() {
    stopped_.store(true);
    pool_.shutdown();
}

void BackupServer::handleConnection(int client_fd) {
    SocketFd client(client_fd);
    while (!stopped_.load()) {
        try {
            const auto packet = PacketCodec::readPacket(client.get());
            handlePacket(client.get(), packet);
        } catch (const DpcError& e) {
            if (std::string(e.what()) != "socket closed") {
                sendError(client.get(), 0, e.what());
            }
            return;
        }
    }
}

void BackupServer::handlePacket(int client_fd, const Packet& packet) {
    if (packet.type == PacketType::Hello) {
        sendText(client_fd, PacketType::Hello, packet.session_id_hash, "DPC_SERVER_V1\n");
        return;
    }

    if (packet.type == PacketType::QuerySession) {
        const auto fields = parseTextFields(payloadString(packet), "DPC_QUERY_SESSION_V1");
        const auto session_id = ManifestStore::hexDecodeString(field(fields, "session_hex"));
        SessionIndex index(repo_, session_id);
        std::ostringstream out;
        out << "DPC_SESSION_STATUS_V1\n";
        for (const auto& [chunk_index, sha] : index.received()) {
            out << chunk_index << " " << sha << "\n";
        }
        sendText(client_fd, PacketType::SessionStatus, packet.session_id_hash, out.str());
        return;
    }

    if (packet.type == PacketType::PutChunk) {
        const auto put = parsePutPayload(packet);
        const auto raw = Compressor::decompress(put.compressed, static_cast<std::size_t>(put.raw_size));
        const auto sha = Hash::sha256Hex(raw);
        const auto header_sha = PacketCodec::shaHexFromArray(packet.chunk_sha256);
        if (sha != header_sha) {
            throw DpcError("PUT_CHUNK checksum mismatch");
        }
        ObjectStore objects(repo_);
        const auto stored = objects.putCompressed(sha, put.compressed, put.raw_size);

        ReceivedChunkRecord record;
        record.global_index = packet.chunk_index;
        record.sha256 = sha;
        record.relative_path = put.relative_path;
        record.file_size = put.file_size;
        record.file_mode = put.file_mode;
        record.file_sha256 = put.file_sha256;
        record.chunk_index = put.chunk_index;
        record.raw_size = put.raw_size;
        record.compressed_size = stored.compressed_size;
        record.object_path = stored.object_path;
        SessionIndex(repo_, put.session_id).appendChunk(record);
        sendText(client_fd, PacketType::PutChunkAck, packet.session_id_hash, "ok\n");
        return;
    }

    if (packet.type == PacketType::CommitSession) {
        const auto fields = parseTextFields(payloadString(packet), "DPC_COMMIT_V1");
        const auto session_id = ManifestStore::hexDecodeString(field(fields, "session_hex"));
        const auto total_chunks = parseU64(fields, "total_chunks");

        ManifestStore manifests(repo_);
        const auto version = manifests.nextVersion();
        WalLog wal(repo_);
        wal.append(WalRecordType::BeginBackup, "network-version=" + std::to_string(version) + "\n");
        auto manifest = SessionIndex(repo_, session_id).buildManifest(version, total_chunks);
        manifests.writeTmp(version, manifest);
        wal.append(WalRecordType::WriteManifest, "version=" + std::to_string(version) + "\n");
        manifests.renameTmpToManifest(version);
        wal.append(WalRecordType::RenameManifest, "version=" + std::to_string(version) + "\n");
        CommitMarker(repo_).create(version);
        wal.append(WalRecordType::CommitBackup, "version=" + std::to_string(version) + "\n");
        writeCheckpoint(repo_);
        SessionIndex(repo_, session_id).markCommitted(version);

        sendText(client_fd, PacketType::CommitAck, packet.session_id_hash, "committed_version " + std::to_string(version) + "\n");
        return;
    }

    throw DpcError("unsupported packet type");
}

}  // namespace dpc
