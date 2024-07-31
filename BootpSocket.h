/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct BootpSocketPrivate;
class BootpSocket
{
    std::unique_ptr<BootpSocketPrivate> mp;
public:
    BootpSocket(std::uint16_t serverPort, std::uint16_t clientPort, std::string deviceName);
    ~BootpSocket();
};
