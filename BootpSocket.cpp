/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "BootpSocket.h"
#include "BootpHandler.h"
#include "Logger.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/ip.h>

#include <cerrno>

#include <thread>
#include <atomic>

namespace
{
constexpr auto ReadBufLen = 512u;
}

struct BootpSocketPrivate
{
    ~BootpSocketPrivate();

    void setupSocket();
    void socketThreadFn();

    std::uint16_t serverPort{ 67 };
    std::uint16_t clientPort{ 68 };
    std::string deviceName;

    std::thread receiverThread;
    std::atomic_bool running{};
    int sockfd{};

    void sendResponse(std::uint32_t target, std::span<const std::uint8_t> data) const
    {
        struct sockaddr_in addr{};
        addr.sin_addr.s_addr = htonl(target);
        addr.sin_port = htons(clientPort);

        Log::Debug("Sending response to {} on {} bytes", convertIpAddress(target), data.size());

        auto bytesSent = sendto(sockfd, data.data(), data.size(), 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        if (bytesSent == 0)
        {
            Log::Warning("Socket sent zero bytes?");
        }
        else if (bytesSent < 0)
        {
            auto e = errno;
            Log::Warning("Socket got write error, errno={}", e);
        }
        else
        {
            Log::Debug("Successfully responded with {} bytes to {}", bytesSent, convertIpAddress(target));
        }
    }
};

BootpSocketPrivate::~BootpSocketPrivate() = default;

void BootpSocketPrivate::setupSocket()
{
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        const auto e = errno;
        Log::Critical("socket() error, errno={}", e);
        running = false;
        return;
    }

    sockaddr_in si_me{};
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(serverPort);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    auto ret = setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, deviceName.c_str(), deviceName.size());
    if (ret != 0)
    {
        const auto e = errno;
        Log::Critical("setsockopt() error, errno={}, ret={}", e, ret);
        running = false;
        return;
    }

    int yes = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &yes, sizeof(yes));
    if (ret != 0)
    {
        auto e = errno;
        Log::Warning("Socket setsockopt SO_DONTROUTE failed, errno={}", e);
    }

    ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    if (ret != 0)
    {
        auto e = errno;
        Log::Warning("Socket setsockopt SO_BROADCAST failed, errno={}", e);
    }

    unsigned int opt = IPTOS_LOWDELAY;
    ret = setsockopt(sockfd, IPPROTO_IP, IP_TOS, &opt, sizeof(opt));
    if (ret != 0)
    {
        auto e = errno;
        Log::Critical("Socket setsockopt IP_TOS failed, errno={}", e);
    }

    ret = bind(sockfd, reinterpret_cast<sockaddr *>(&si_me), sizeof(si_me));
    if (ret != 0)
    {
        const auto e = errno;
        Log::Critical("bind() error, errno={}, ret={}", e, ret);
        running = false;
        return;
    }
}

void BootpSocketPrivate::socketThreadFn()
{
    setupSocket();

    Log::Info("Started Bootp receiver thread for {}", deviceName);

    while (running)
    {
        fd_set readfds;

        FD_ZERO(&readfds); //Zero out socket set
        FD_SET(sockfd, &readfds); //Add socket to listen to

        struct timeval timeout{};
        timeout.tv_sec = 1;
        const auto select_ret = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout);

        auto response = BootpHandler::getNextResponse();
        if (response)
        {
            sendResponse(response->target, response->data);
        }

        if (select_ret < 1)
            continue;

        struct sockaddr_in addr{};
        socklen_t addrlen = sizeof(addr);
        std::uint8_t data[ReadBufLen]{};

        auto ret = recvfrom(sockfd, data, ReadBufLen, 0, reinterpret_cast<sockaddr*>(&addr), &addrlen);
        if (ret < 0)
        {
            const auto e = errno;
            Log::Warning("Socket read error, errno={}", e);
            continue;
        }

        std::vector<std::uint8_t> dataVector;
        dataVector.insert(dataVector.end(), data, data + ret);
        Log::Debug("Socket got data on adapter {} ({} bytes)", deviceName, dataVector.size());

        BootpHandler::addRequestData(deviceName, std::move(dataVector));
    }

    ::close(sockfd);
}

BootpSocket::BootpSocket(std::uint16_t serverPort, std::uint16_t clientPort, std::string deviceName)
{
    mp = std::make_unique<BootpSocketPrivate>();
    mp->serverPort = serverPort;
    mp->clientPort = clientPort;
    mp->deviceName = std::move(deviceName);
    mp->running = true;
    mp->receiverThread = std::thread(&BootpSocketPrivate::socketThreadFn, mp.get());
}

BootpSocket::~BootpSocket()
{
    Log::Info("Destroying Bootp socket for {}", mp->deviceName);
    mp->running = false;
    if (mp->receiverThread.joinable())
        mp->receiverThread.join();
}
