#pragma once

#include "dpc/concurrency/ThreadPool.hpp"
#include "dpc/network/PacketCodec.hpp"

#include <atomic>
#include <filesystem>

namespace dpc {

class BackupServer {
public:
    BackupServer(std::filesystem::path repo, int port, std::size_t workers);

    void run();
    void stop();

private:
    void handleConnection(int client_fd);
    void handlePacket(int client_fd, const Packet& packet);

    std::filesystem::path repo_;
    int port_;
    ThreadPool pool_;
    std::atomic<bool> stopped_ {false};
};

}  // namespace dpc
