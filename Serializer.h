/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include "Structures.h"
#include <vector>
#include <span>
#include <cstdint>

/// Serializes the given BOOTP structure into a buffer of bytes.
std::vector<std::uint8_t> serializeBootp(const BOOTP&);

/// Tries to de-serialize a buffer of bytes into a BOOTP structure. Returns false upon error.
bool deserializeBootp(std::span<const std::uint8_t>, BOOTP&);
