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

#include <vector>
#include <optional>
#include <string>
#include <span>

struct BootpResponse
{
    std::uint32_t target;
    std::vector<std::uint8_t> data;
};

namespace BootpHandler
{
    void start(std::unordered_map<std::string, Network>&&);
    void stop();
    void addRequestData(std::string deviceSource, std::span<const std::uint8_t> data);
    std::optional<BootpResponse> getNextResponse();
}
