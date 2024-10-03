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

class Network
{
public:
    void configure(NetworkConfiguration&& config, const std::vector<Lease>& leases = {});

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

    void setLeaseDuration(std::uint32_t leaseTimeSeconds);

    std::uint32_t getBroadcastAddress() const;

    std::uint32_t getLeaseTime() const;

    std::uint32_t getRenewalTime() const;

    std::uint32_t getRebindingTime() const;

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
    std::uint32_t m_networkSpace{ NetworkDefaults::space };
    std::uint8_t m_networkSize{ NetworkDefaults::size };
    std::uint32_t m_routers{ NetworkDefaults::routers };
    std::uint32_t m_dhcpServerIdentifier{ NetworkDefaults::serverIdentifier };
    std::uint32_t m_dhcpFirst{ NetworkDefaults::first };
    std::uint32_t m_dhcpLast{ NetworkDefaults::last };
    std::vector<std::uint32_t> m_dnsServers;
    std::uint32_t m_leaseTime{ NetworkDefaults::leaseTime };
    std::uint32_t m_renewalTime{ NetworkDefaults::renewalTime };
    std::uint32_t m_rebindingTime{ NetworkDefaults::rebindingTime };
    std::string m_leaseFile;

    std::unordered_map<std::uint64_t, std::uint32_t> m_reservationByHw;
    std::unordered_map<std::uint32_t, std::uint64_t> m_reservationByIp;

    std::unordered_map<std::uint64_t, Lease> m_leasesByHw;
    std::unordered_map<std::uint32_t, Lease> m_leasesByIp;

    const Lease m_invalidLease;

    bool isIpAllowed(std::uint32_t ipAddress) const;

    bool isIpReservedInConfig(std::uint32_t ipAddress) const;

    void addLease(std::uint64_t hwAddress, std::uint32_t ipAddress);

    void removeLease(std::uint64_t hwAddress);

    void removeLease(std::uint32_t ipAddress);
};
