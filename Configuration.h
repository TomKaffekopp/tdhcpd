/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include "Structures.h"
#include "IpConverter.h"
#include "Logger.h"

#include <string_view>
#include <vector>
#include <span>
#include <optional>

/*
 * Defaults mostly meant for unit tests.
*/
namespace NetworkDefaults
{
    constexpr auto space{ concatenateIpAddress(192, 168, 200, 0) };
    constexpr auto size{ 24 };
    constexpr auto routers{ concatenateIpAddress(192, 168, 200, 1) };
    constexpr auto serverIdentifier{ concatenateIpAddress(192, 168, 200, 1) };
    constexpr auto first{ concatenateIpAddress(192, 168, 200, 100) };
    constexpr auto last{ concatenateIpAddress(192, 168, 200, 254) };
    constexpr auto leaseTime{ 3600 };
}

struct NetworkConfiguration
{
    std::uint32_t networkSpace{ NetworkDefaults::space };
    std::uint8_t networkSize{ NetworkDefaults::size };
    std::uint32_t routers{ NetworkDefaults::routers };
    std::uint32_t dhcpServerIdentifier{ NetworkDefaults::serverIdentifier };
    std::uint32_t dhcpFirst{ NetworkDefaults::first };
    std::uint32_t dhcpLast{ NetworkDefaults::last };
    std::vector<std::uint32_t> dnsServers;
    std::uint32_t leaseTime{ NetworkDefaults::leaseTime };
    std::string leaseFile;
    std::unordered_map<std::uint64_t, std::uint32_t> reservations;
};

namespace Configuration
{
    bool LoadFromFile(const std::string& path);

    std::vector<std::string> GetConfiguredInterfaces();

    NetworkConfiguration GetNetworkConfiguration(const std::string& interface);

    std::vector<Lease> GetPersistentLeasesByInterface(const std::string& interface);

    std::vector<Lease> GetPersistentLeasesByFile(const std::string& filename);

    void SavePersistentLeases(const std::vector<Lease>& leases, const std::string& leaseFile);

    const std::string& GetPidFileName();

    const std::string& GetLogFileName();

    Log::Level GetLogLevel();
}
