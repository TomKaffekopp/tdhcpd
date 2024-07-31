/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "IpConverter.h"
#include "Logger.h"

#include <format>
#include <stdexcept>

std::uint32_t concatenateIpAddress(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d)
{
    std::uint32_t ret = a;

    ret <<= 8;
    ret |= b;

    ret <<= 8;
    ret |= c;

    ret <<= 8;
    ret |= d;

    return ret;
}

std::uint32_t convertIpAddress(std::string_view address) try
{
    std::uint8_t part[4]{};

    if (address.empty())
        return 0;

    for (std::uint8_t& i : part)
    {
        auto pos = address.find('.');
        std::string str(address.substr(0, pos));
        i = std::stoi(str);
        address = address.substr(pos + 1);
    }

    return concatenateIpAddress(part[0], part[1], part[2], part[3]);
}
catch (const std::invalid_argument&)
{
    Log::Warning("Trying to convert {} to integer failed! Part of the address is not a number", address);
    return 0;
}
catch (const std::out_of_range&)
{
    // This one kinda doesn't make much sense, since stoi() allows 32-bit numbers, but each "part" of IP address string is 8... oh well.
    Log::Warning("Trying to convert {} to integer failed! Part of the address is too large", address);
    return 0;
}

std::string convertIpAddress(std::uint32_t address)
{
    auto ret = std::to_string((address & 0xFF000000) >> 24) + "."
             + std::to_string((address & 0x00FF0000) >> 16) + "."
             + std::to_string((address & 0x0000FF00) >> 8 ) + "."
             + std::to_string( address & 0x000000FF);
    return ret;
}

std::string convertHardwareAddress(std::uint64_t address)
{
    auto toHex = [](unsigned long long value)
    {
        return std::format("{:02X}", value);
    };

    auto ret = toHex((address & 0x0000FF0000000000ull) >> 40) + ":"
             + toHex((address & 0x000000FF00000000ull) >> 32) + ":"
             + toHex((address & 0x00000000FF000000ull) >> 24) + ":"
             + toHex((address & 0x0000000000FF0000ull) >> 16) + ":"
             + toHex((address & 0x000000000000FF00ull) >> 8 ) + ":"
             + toHex( address & 0x00000000000000FFull);
    return ret;
}
