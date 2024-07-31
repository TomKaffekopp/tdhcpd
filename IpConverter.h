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

// Concatenates 4 separate bytes to a single 4 byte integer. For example: concatenateIpAddress(192, 168, 1, 23);
std::uint32_t concatenateIpAddress(std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t);

// Converts a string representation of an IP address to a single 4 byte integer.
std::uint32_t convertIpAddress(std::string_view address);

// Converts a 4 byte integer to a string representation.
std::string convertIpAddress(std::uint32_t address);

// Converts a hardware address (MAC) represented as 64bit int to 6 byte hexadecimal string separated by colons.
std::string convertHardwareAddress(std::uint64_t address);
