/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include "Network.h"

#include <unordered_map>
#include <netinet/in.h>

#include <cstdint>

#include <optional>
#include <string>
#include <span>
#include <memory>

struct BootpResponse
{
    std::uint32_t target;
    std::vector<std::uint8_t> data;
};

struct BootpHandlerPrivate;
class BootpHandler
{
    std::unique_ptr<BootpHandlerPrivate> mp;
public:
    explicit BootpHandler(std::string deviceName);
    ~BootpHandler();
    std::optional<BootpResponse> handleRequest(std::span<const std::uint8_t> data);
};
