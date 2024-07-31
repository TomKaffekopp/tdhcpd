/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include "Structures.h"
#include "Logger.h"

#include <string_view>
#include <vector>
#include <span>
#include <optional>

struct NetworkConfiguration
{
    std::uint32_t networkSpace;
    std::uint8_t networkSize;
    std::uint32_t routers;
    std::uint32_t dhcpServerIdentifier;
    std::uint32_t dhcpFirst;
    std::uint32_t dhcpLast;
    std::vector<std::uint32_t> dnsServers;
    std::uint32_t leaseTime;
    std::string leaseFile;
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
