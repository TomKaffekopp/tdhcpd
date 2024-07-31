/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "Serializer.h"
#include "Logger.h"

#include <string>

namespace
{
template<typename T>
void addIntegerToBufferAsBigEndian(T value, std::vector<std::uint8_t>& buffer)
{
    for (auto i = 0u; i < sizeof(T); ++i)
    {
        const auto shift = sizeof(T) * 8 - (i + 1) * 8;
        const T mask = static_cast<T>(0xFF) << shift;
        buffer.emplace_back( (value & mask) >> shift );
    }
}

template<typename T>
T readBigEndianIntegerFromBuffer(std::span<const std::uint8_t> buffer, std::uint32_t offset) try
{
    T value{};
    buffer = buffer.subspan(offset, sizeof(T));
    for (auto n : buffer)
    {
        if constexpr (sizeof(T) > 1)
            value <<= 8;
        value |= n;
    }
    return value;
}
catch (...)
{
    // TODO log this
    return 0;
}

bool deserializeIpList(std::span<const std::uint8_t> buffer, std::vector<std::uint32_t>& ipList)
{
    const auto count = buffer.front() / 4; // 4 bytes per IP.
    buffer = buffer.subspan(1);

    for (int i = 0; i < count; ++i)
    {
        if (!buffer.empty() && buffer.size() < 4)
            return false; // TODO log this

        ipList.emplace_back( readBigEndianIntegerFromBuffer<std::uint32_t>(buffer, 0) );
        buffer = buffer.subspan(4);
    }

    return true;
}

void deserializeParameterList(std::span<const std::uint8_t> buffer, std::vector<BOOTPOptionKey>& parameters)
{
    const auto count = buffer.front();
    parameters.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        // adjusting by 1 since index 0 holds the count.
        parameters.emplace_back(static_cast<BOOTPOptionKey>(buffer[i + 1]));
    }
}

bool deserializeBootpOptions(std::span<const std::uint8_t> buffer, BOOTP& bootp)
{
    while (!buffer.empty())
    {
        const auto option = static_cast<BOOTPOptionKey>(buffer.front());
        buffer = buffer.subspan(1);

        switch (option)
        {
            case Option_Pad:
                continue;

            case Option_SubnetMask:       [[fallthrough]];
            case Option_Router:           [[fallthrough]];
            case Option_DomainNameServer: [[fallthrough]];
            case Option_BroadcastAddress: [[fallthrough]];
            case Option_RequestedIp:
            {
                std::vector<std::uint32_t> ipList;
                if (!deserializeIpList(buffer, ipList))
                    return false;
                bootp.options[option] = std::make_unique<IpListBOOTPOption>(std::move(ipList));
                break;
            }
            case Option_ParameterRequestList:
            {
                std::vector<BOOTPOptionKey> parameters;
                deserializeParameterList(buffer, parameters);
                bootp.options[option] = std::make_unique<ParameterListBOOTPOption>(std::move(parameters));
                break;
            }
            case Option_MessageType:
            {
                if (buffer.front() != 1) // currently the only size we expect from this message.
                {
                    Log::Warning("Expected MessageType option with size 1 but got {}", buffer.front());
                    return false;
                }
                auto messageType = static_cast<DHCPMessageType>(buffer[1]);
                bootp.options[option] = std::make_unique<DHCPMessageTypeBOOTPOption>(messageType);
                break;
            }
            case Option_End:
                return true;

            /* not applicable: */
            case Option_IPLeaseTime: break;
            case Option_ServerIdentifier: break;
        }

        buffer = buffer.subspan(buffer.front() + 1);
    }

    return false;
}
}

std::vector<std::uint8_t> serializeBootp(const BOOTP& bootp)
{
    std::vector<std::uint8_t> data;
    data.reserve(512);

    data.emplace_back(bootp.operation);
    data.emplace_back(bootp.hardwareType);
    data.emplace_back(bootp.hardwareAddressLength);
    data.emplace_back(bootp.hops);

    addIntegerToBufferAsBigEndian(bootp.transactionId, data);
    addIntegerToBufferAsBigEndian(bootp.secondsElapsed, data);
    addIntegerToBufferAsBigEndian(bootp.flags, data);
    addIntegerToBufferAsBigEndian(bootp.ciaddr, data);
    addIntegerToBufferAsBigEndian(bootp.yiaddr, data);
    addIntegerToBufferAsBigEndian(bootp.siaddr, data);
    addIntegerToBufferAsBigEndian(bootp.giaddr, data);

    /* left shifting 2 bytes since MAC is 6 bytes, 64bit int is 8. Also needs some padding. */
    addIntegerToBufferAsBigEndian(bootp.chaddr << 16, data);
    addIntegerToBufferAsBigEndian<std::uint64_t>(0ull, data);

    /* unused portion of BOOTP, 64 + 128 bytes. */
    static const std::vector<std::uint8_t> padding(64 + 128, 0);
    data.insert(data.end(), padding.begin(), padding.end());

    addIntegerToBufferAsBigEndian(bootp.magic, data);

    /* Let's place these on the top of options list, for convenience. */
    try
    {
        const auto& messageTypeOption = bootp.options.at(Option_MessageType);
        auto messageTypeData = messageTypeOption->serialize();
        data.emplace_back(Option_MessageType);
        data.insert(data.end(), messageTypeData.begin(), messageTypeData.end());

        const auto& serverIdentifierOption = bootp.options.at(Option_ServerIdentifier);
        auto serverIdentifierData = serverIdentifierOption->serialize();
        data.emplace_back(Option_ServerIdentifier);
        data.insert(data.end(), serverIdentifierData.begin(), serverIdentifierData.end());
    }
    catch (const std::out_of_range&)
    {
        Log::Critical("Serialization error! This would be a bug - MessageType or ServerIdentifier is missing, message will not be sent.");
        return {};
    }

    for (const auto& [key, parameter] : bootp.options)
    {
        if (key == Option_MessageType || key == Option_ServerIdentifier)
            continue; // already handled.

        data.emplace_back(key);
        auto parameterData = parameter->serialize();
        data.insert(data.end(), parameterData.begin(), parameterData.end());
    }

    data.emplace_back(Option_End);

    /* Apparently 300 bytes is a minimum size that DHCP packet should be...? */
    int remainingSize = 300 - static_cast<int>(data.size());
    if (remainingSize > 0)
    {
        // Add padding of zeroes on the end.
        std::vector<std::uint8_t> endpadding(remainingSize, 0);
        data.insert(data.end(), endpadding.begin(), endpadding.end());
    }

    return data;
}

bool deserializeBootp(std::span<const std::uint8_t> data, BOOTP& bootp)
{
    if (data.size() < 241)
        return false; // 241 bytes appear to be the absolute smallest size of any DHCP message.

    bootp.magic = readBigEndianIntegerFromBuffer<std::uint32_t>(data, 236);
    if (bootp.magic != 0x63825363)
        return false; // TODO Log this

    bootp.operation = static_cast<BOOTPOperation>(data[0]);
    bootp.hardwareType = data[1];
    bootp.hardwareAddressLength = data[2];
    bootp.hops = data[3];

    bootp.transactionId = readBigEndianIntegerFromBuffer<std::uint32_t>(data, 4);
    bootp.secondsElapsed = readBigEndianIntegerFromBuffer<std::uint16_t>(data, 8);
    bootp.flags = readBigEndianIntegerFromBuffer<std::uint16_t>(data, 10);
    bootp.ciaddr = readBigEndianIntegerFromBuffer<std::uint32_t>(data, 12);
    bootp.yiaddr = readBigEndianIntegerFromBuffer<std::uint32_t>(data, 16);
    bootp.siaddr = readBigEndianIntegerFromBuffer<std::uint32_t>(data, 20);
    bootp.giaddr = readBigEndianIntegerFromBuffer<std::uint32_t>(data, 24);

    // Note that we need to shift the MAC address 16 bits over to the right due to it being 6 byte number versus uint64's 8 byte number.
    bootp.chaddr = readBigEndianIntegerFromBuffer<std::uint64_t>(data, 28) >> 16;

    return deserializeBootpOptions(data.subspan(240), bootp);
}
