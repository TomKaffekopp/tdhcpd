/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "Configuration.h"
#include "IpConverter.h"
#include "Logger.h"

#include <cstring>

#include <fstream>
#include <string_view>

namespace
{
std::string PidFileName;
std::unordered_map<std::string,NetworkConfiguration> Configs;
std::string LogFileName;
Log::Level LogLevel{Log::Level::Info };

std::vector<std::string> parseParameterList(std::string_view val)
{
    // config_key parameter_one parameter_two parameter_3 ....
    //            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ -- This part.

    auto consumeParameter = [&val]
    {
        if (val.empty())
            return std::string();

        auto end = val.find_first_of(" \t");
        std::string ret(val.substr(0, end));
        if (end != std::string::npos)
            val = val.substr(end);
        else
            val = {};
        return ret;
    };

    auto consumeWhitespace = [&val]
    {
        if (val.empty())
            return;
        auto end = val.find_first_not_of(" \t");
        if (end != std::string::npos)
            val = val.substr(end);
    };

    std::vector<std::string> parameterList;

    while (!val.empty())
    {
        auto parameter = consumeParameter();
        consumeWhitespace();
        parameterList.emplace_back(std::move(parameter));
    }

    return parameterList;
}

bool handleConfig_network(std::string_view val, NetworkConfiguration& config) try
{
    // network 192.168.200.0/24

    auto splitpos = val.find('/');
    if (splitpos == std::string::npos)
    {
        Log::Critical("Configuration error: Network must be specified with CIDR");
        return false;
    }

    bool ok{};
    config.networkSpace = convertIpAddress(val.substr(0, splitpos), ok);
    config.networkSize = std::stoi(std::string(val.substr(splitpos + 1)));
    return ok && config.networkSpace != 0;
}
catch (...)
{
    Log::Critical("Configuration error: Network must be specified with CIDR");
    return false;
}

bool handleConfig_routers(std::string_view val, NetworkConfiguration& config)
{
    // routers 192.168.200.1

    bool ok{};
    config.routers = convertIpAddress(val, ok);
    return ok && config.routers != 0;
}

bool handleConfig_serverid(std::string_view val, NetworkConfiguration& config)
{
    // serverid 192.168.200.1

    bool ok{};
    config.dhcpServerIdentifier = convertIpAddress(val, ok);
    return ok && config.dhcpServerIdentifier != 0;
}

bool handleConfig_dhcp_first(std::string_view val, NetworkConfiguration& config)
{
    // dhcp_first 192.168.200.100

    bool ok{};
    config.dhcpFirst = convertIpAddress(val, ok);
    return ok && config.dhcpFirst != 0;
}

bool handleConfig_dhcp_last(std::string_view val, NetworkConfiguration& config)
{
    // dhcp_last 192.168.200.254

    bool ok{};
    config.dhcpLast = convertIpAddress(val, ok);
    return ok && config.dhcpLast != 0;
}

bool handleConfig_dns_servers(std::string_view val, NetworkConfiguration& config)
{
    // dns_servers serverip1 serverip2

    auto parameterList = parseParameterList(val);

    for (const auto& address : parameterList)
    {
        bool ok{};

        config.dnsServers.emplace_back(convertIpAddress(address, ok));

        if (!ok)
            return false;
    }

    return !config.dnsServers.empty();
}

bool handleConfig_lease_time(std::string_view val, NetworkConfiguration& config)
{
    // lease_time 86400

    config.leaseTime = std::stoi(std::string(val));
    return config.leaseTime > 0;
}

bool handleConfig_renewal_time(std::string_view val, NetworkConfiguration& config)
{
    // renewal_time 43200

    config.renewalTime = std::stoi(std::string(val));
    return config.renewalTime > 0;
}

bool handleConfig_rebinding_time(std::string_view val, NetworkConfiguration& config)
{
    // rebinding_time 75600

    config.rebindingTime = std::stoi(std::string(val));
    return config.rebindingTime > 0;
}

bool handleConfig_lease_file(std::string_view val, NetworkConfiguration& config)
{
    // lease_file /var/tdhcpd/eth0.lease

    config.leaseFile = val;
    return !config.leaseFile.empty();
}

bool handleConfig_reserve(std::string_view val, NetworkConfiguration& config)
{
    // reserve 11:22:33:44:55:66 192.168.200.123

    auto parameterList = parseParameterList(val);
    if (parameterList.size() != 2)
        return false;

    const auto& hwaddrStr = parameterList[0];
    const auto& ipaddrStr = parameterList[1];

    bool hwok{};
    bool ipok{};

    auto hwaddr = convertHardwareAddress(hwaddrStr, hwok);
    auto ipaddr = convertIpAddress(ipaddrStr, ipok);

    if (!hwok || !ipok)
        return false;

    config.reservations[hwaddr] = ipaddr;
    return true;
}

bool handleConfigEntry(std::string_view key, std::string_view val, NetworkConfiguration& config)
{
    if (key == "network")
        return handleConfig_network(val, config);

    else if (key == "routers")
        return handleConfig_routers(val, config);

    else if (key == "serverid")
        return handleConfig_serverid(val, config);

    else if (key == "dhcp_first")
        return handleConfig_dhcp_first(val, config);

    else if (key == "dhcp_last")
        return handleConfig_dhcp_last(val, config);

    else if (key == "dns_servers")
        return handleConfig_dns_servers(val, config);

    else if (key == "lease_time")
        return handleConfig_lease_time(val, config);

    else if (key == "renewal_time")
        return handleConfig_renewal_time(val, config);

    else if (key == "rebinding_time")
        return handleConfig_rebinding_time(val, config);

    else if (key == "lease_file")
        return handleConfig_lease_file(val, config);

    else if (key == "reserve")
        return handleConfig_reserve(val, config);

    Log::Critical("Configuration error: Unknown config key {}", key);
    return false;
}
} // Anonymous ns


bool Configuration::LoadFromFile(const std::string& path)
{
    auto stripCommentAndWhitespace = [](std::string_view input) -> std::string_view
    {
        auto end = input.find('#');
        input = input.substr(0, end);

        auto start = input.find_first_not_of(" \t");
        if (start != std::string::npos)
            input = input.substr(start);

        if (input.empty())
            return input;

        unsigned int trailingSpaceCount{};
        while (trailingSpaceCount < input.size()
               && (input.at(input.size() - trailingSpaceCount - 1) == ' '
                   || input.at(input.size() - trailingSpaceCount - 1) == '\t'))
        {
            ++trailingSpaceCount;
        }

        if (trailingSpaceCount > 0)
            input = input.substr(0, input.size() - trailingSpaceCount);

        return input;
    };

    auto getKey = [](std::string_view input) -> std::string_view
    {
        auto end = input.find_first_of(" \t");
        return input.substr(0, end);
    };

    auto getVal = [](std::string_view input) -> std::string_view
    {
        auto start = input.find_first_of(" \t");
        while (start < input.size() && (input.at(start) == ' ' || input.at(start) == '\t'))
            ++start;

        if (start >= input.size())
            return {};

        return input.substr(start);
    };

    NetworkConfiguration* current = nullptr;

    std::ifstream ifs(path);

    char buf[1024]{};
    while (ifs.getline(buf, sizeof(buf)))
    {
        auto line = stripCommentAndWhitespace(buf);
        if (line.empty())
            continue;

        auto key = getKey(line);
        auto val = getVal(line);

        if (key == "interface")
        {
            current = &(Configs[std::string(val)]);
            continue;
        }
        else if (key == "pidfile")
        {
            PidFileName = val;
            continue;
        }
        else if (key == "logfile")
        {
            LogFileName = val;
            continue;
        }
        else if (key == "loglevel")
        {
            LogLevel = Log::ToLogLevel(val);
            continue;
        }

        if (!current)
        {
            // log error that "interface" is not yet specified.
            Log::Critical("Configuration error: 'interface' not defined before reading {}", key);
            Configs.clear();
            break;
        }

        if (!handleConfigEntry(key, val, *current))
        {
            Configs.clear();
            break;
        }
    }

    if (Configs.empty())
    {
        Log::Critical("Error while reading configuration!");
        return false;
    }

    /*
     * Handle optional parameters (except "reserve")
    */
    for (auto& [interface, config] : Configs)
    {
        if (config.renewalTime == NetworkDefaults::renewalTime)
            config.renewalTime = static_cast<std::uint32_t>(config.leaseTime * 0.5);

        if (config.rebindingTime == NetworkDefaults::rebindingTime)
            config.rebindingTime = static_cast<std::uint32_t>(config.leaseTime * 0.875);

        if (config.renewalTime >= config.rebindingTime)
        {
            Log::Critical("Parameter renewal_time must be less than rebinding_time for interface {}", interface);
            return false;
        }

        if (config.rebindingTime >= config.leaseTime)
        {
            Log::Critical("Parameter rebinding_time must be less than lease_time for interface {}", interface);
            return false;
        }
    }

    return true;
}

std::vector<std::string> Configuration::GetConfiguredInterfaces()
{
    std::vector<std::string> interfaces;
    interfaces.reserve(Configs.size());

    for (const auto& [interface, config] : Configs)
    {
        interfaces.emplace_back(interface);
    }

    return interfaces;
}

NetworkConfiguration Configuration::GetNetworkConfiguration(const std::string& interface)
{
    return Configs[interface];
}

std::vector<Lease> Configuration::GetPersistentLeasesByInterface(const std::string& interface)
{
    const auto& filename = Configs[interface].leaseFile;
    if (filename.empty())
        return {};

    return GetPersistentLeasesByFile(filename);
}

std::vector<Lease> Configuration::GetPersistentLeasesByFile(const std::string& filename)
{
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (!ifs.is_open())
        return {};

    char buffer[1024]{};
    std::vector<std::uint8_t> data;

    while (ifs.good() && !ifs.eof())
    {
        ifs.read(buffer, sizeof(buffer));
        auto readCount = ifs.gcount();

        if (readCount > 0)
            data.insert(data.end(), buffer, buffer + readCount);
    }

    std::vector<Lease> leases;
    leases.reserve(16);

    auto readLease = [&data](int index, Lease& lease)
    {
        const auto offset = index * LeaseLen;
        if (offset + LeaseLen - 1 > data.size())
            return false; // EOF

        memcpy(&lease.startTime, data.data() + offset, StartTimeLen);
        memcpy(&lease.hwAddress, data.data() + offset + StartTimeLen, HwAddressLen);
        memcpy(&lease.ipAddress, data.data() + offset + StartTimeLen + HwAddressLen, IpAddressLen);

        return true; // More to read
    };

    bool readNextLease = true;
    int index = 0;
    while (readNextLease)
    {
        auto& lease = leases.emplace_back();
        readNextLease = readLease(index, lease);
        if (lease.startTime == 0)
            leases.pop_back();
        ++index;
    }

    return leases;
}

void Configuration::SavePersistentLeases(const std::vector<Lease>& leases, const std::string& leaseFile)
{
    std::vector<std::uint8_t> data;
    data.reserve(320);

    for (const auto& lease : leases)
    {
        const auto* startTimePtr = reinterpret_cast<const std::uint8_t*>(&lease.startTime);
        data.insert(data.end(), startTimePtr, startTimePtr + StartTimeLen);

        const auto* hwAddressPtr = reinterpret_cast<const std::uint8_t*>(&lease.hwAddress);
        data.insert(data.end(), hwAddressPtr, hwAddressPtr + HwAddressLen);

        const auto* ipAddressPtr = reinterpret_cast<const std::uint8_t*>(&lease.ipAddress);
        data.insert(data.end(), ipAddressPtr, ipAddressPtr + IpAddressLen);
    }

    std::ofstream ofs(leaseFile);
    if (!ofs.is_open())
    {
        Log::Warning("Couldn't write to lease file {}", leaseFile);
        return;
    }

    ofs.write(reinterpret_cast<const char*>(data.data()),
              static_cast<long>(data.size()));

    ofs.close();
}

const std::string& Configuration::GetPidFileName()
{
    return PidFileName;
}

const std::string& Configuration::GetLogFileName()
{
    return LogFileName;
}

Log::Level Configuration::GetLogLevel()
{
    return LogLevel;
}
