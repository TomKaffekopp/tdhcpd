/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include <string>
#include <string_view>
#include <cstdint>

// Concatenates 4 separate bytes to a single 4 byte (32 bit) integer. For example: concatenateIpAddress(192, 168, 1, 23);
constexpr std::uint32_t concatenateIpAddress(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d)
{
    std::uint32_t ret{};

    auto concatenate = [](std::uint8_t n, std::uint32_t& ret)
    {
        ret <<= 8;
        ret |= n;
    };

    concatenate(a, ret);
    concatenate(b, ret);
    concatenate(c, ret);
    concatenate(d, ret);

    return ret;
}

// Concatenates 6 separate bytes to a single 8 byte (64 bit) integer. For example: concatenateHardwareAddress(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
constexpr std::uint64_t concatenateHardwareAddress(std::uint8_t a, std::uint8_t b, std::uint8_t c,
                                                   std::uint8_t d, std::uint8_t e, std::uint8_t f)
{
    std::uint64_t ret{};

    auto concatenate = [](std::uint8_t n, std::uint64_t& ret)
    {
        ret <<= 8;
        ret |= n;
    };

    concatenate(a, ret);
    concatenate(b, ret);
    concatenate(c, ret);
    concatenate(d, ret);
    concatenate(e, ret);
    concatenate(f, ret);

    return ret;
}

// Converts a string representation of an IP address to a single 4 byte (32bit) integer.
std::uint32_t convertIpAddress(std::string_view address, bool& ok);

// Converts a 4 byte integer to a string representation.
std::string convertIpAddress(std::uint32_t address);

// Converts a hardware address (MAC) represented as a 6 byte hexadecimal string separated by colons to a single 8 byte (64bit) integer.
std::uint64_t convertHardwareAddress(std::string_view address, bool& ok);

// Converts a hardware address (MAC) represented as 64bit int to 6 byte hexadecimal string separated by colons.
std::string convertHardwareAddress(std::uint64_t address);
