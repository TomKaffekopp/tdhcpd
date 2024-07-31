/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include "IpConverter.h"
#include "Structures.h"
#include "Configuration.h"

#include <cstdint>
#include <ctime>
#include <vector>
#include <unordered_map>
#include <mutex>

class Network
{
public:
    void configure(NetworkConfiguration&& config, const std::vector<Lease>& leases);

    void setNetworkSpace(std::uint32_t networkSpace);

    std::uint32_t getNetworkSpace() const;

    void setNetworkSize(std::uint8_t networkSize);

    std::uint8_t getNetworkSize() const;

    void setRouterAddress(std::uint32_t routerAddress);

    std::uint32_t getRouterAddress() const;

    void setDhcpServerIdentifier(std::uint32_t identifier);

    std::uint32_t getDhcpServerIdentifier() const;

    void setDnsServers(std::vector<std::uint32_t> servers);

    const std::vector<std::uint32_t>& getDnsServers() const;

    void setDhcpRange(std::uint32_t first, std::uint32_t last);

    void setLeaseTime(std::uint32_t leaseTimeSeconds);

    std::uint32_t getBroadcastAddress() const;

    std::uint32_t getLeaseTime() const;

    const std::string& getLeaseFile() const;

    std::vector<Lease> getAllLeases() const;

    const Lease& getLease(std::uint64_t hwAddress) const;

    const Lease& getLease(std::uint32_t ipAddress) const;

    std::uint32_t getAvailableAddress(std::uint64_t hardwareAddress, std::uint32_t preferredIpAddress = 0);

    bool reserveAddress(std::uint64_t hardwareAddress, std::uint32_t ipAddress);

    void releaseAddress(std::uint32_t ipAddress);

    static bool isLeaseEntryValid(const Lease& lease);

    bool isLeaseExpired(const Lease& lease) const;

private:
    /* Unit tests depend on the default values provided here. */
    std::uint32_t m_networkSpace{ concatenateIpAddress(192, 168, 200, 0) };
    std::uint8_t m_networkSize{ 24 };
    std::uint32_t m_routers{ concatenateIpAddress(192, 168, 200, 1) };
    std::uint32_t m_dhcpServerIdentifier{ concatenateIpAddress(192, 168, 200, 1) };
    std::uint32_t m_dhcpFirst{ concatenateIpAddress(192, 168, 200, 100) };
    std::uint32_t m_dhcpLast{ concatenateIpAddress(192, 168, 200, 254) };
    std::vector<std::uint32_t> m_dnsServers;
    std::uint32_t m_leaseTime{ 3600 };
    std::string m_leaseFile;

    std::unordered_map<std::uint64_t, Lease> m_leasesByHw;
    std::unordered_map<std::uint32_t, Lease> m_leasesByIp;

    mutable std::mutex m_leasesMutex; // todo this might not be needed.

    const Lease m_invalidLease;

    bool isIpAllowed(std::uint32_t ipAddress) const;

    void addLease(std::uint64_t hwAddress, std::uint32_t ipAddress);

    void removeLease(std::uint64_t hwAddress);

    void removeLease(std::uint32_t ipAddress);
};
