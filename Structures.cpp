/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "Structures.h"

BOOTP::BOOTP(const BOOTP& other)
{
    operation             = other.operation;
    hardwareType          = other.hardwareType;
    hardwareAddressLength = other.hardwareAddressLength;
    hops                  = other.hops;
    transactionId         = other.transactionId;
    secondsElapsed        = other.secondsElapsed;
    flags                 = other.flags;
    ciaddr                = other.ciaddr;
    yiaddr                = other.yiaddr;
    siaddr                = other.siaddr;
    giaddr                = other.giaddr;
    chaddr                = other.chaddr;
    magic                 = other.magic;
}

BOOTP::BOOTP(BOOTP&& other) noexcept
{
    operation             = other.operation;
    hardwareType          = other.hardwareType;
    hardwareAddressLength = other.hardwareAddressLength;
    hops                  = other.hops;
    transactionId         = other.transactionId;
    secondsElapsed        = other.secondsElapsed;
    flags                 = other.flags;
    ciaddr                = other.ciaddr;
    yiaddr                = other.yiaddr;
    siaddr                = other.siaddr;
    giaddr                = other.giaddr;
    chaddr                = other.chaddr;
    magic                 = other.magic;
    options               = std::move(other.options);
}

BOOTP& BOOTP::operator=(const BOOTP& other)
{
    operation             = other.operation;
    hardwareType          = other.hardwareType;
    hardwareAddressLength = other.hardwareAddressLength;
    hops                  = other.hops;
    transactionId         = other.transactionId;
    secondsElapsed        = other.secondsElapsed;
    flags                 = other.flags;
    ciaddr                = other.ciaddr;
    yiaddr                = other.yiaddr;
    siaddr                = other.siaddr;
    giaddr                = other.giaddr;
    chaddr                = other.chaddr;
    magic                 = other.magic;
    return *this;
}

BOOTP& BOOTP::operator=(BOOTP&& other) noexcept
{
    operation             = other.operation;
    hardwareType          = other.hardwareType;
    hardwareAddressLength = other.hardwareAddressLength;
    hops                  = other.hops;
    transactionId         = other.transactionId;
    secondsElapsed        = other.secondsElapsed;
    flags                 = other.flags;
    ciaddr                = other.ciaddr;
    yiaddr                = other.yiaddr;
    siaddr                = other.siaddr;
    giaddr                = other.giaddr;
    chaddr                = other.chaddr;
    magic                 = other.magic;
    options               = std::move(other.options);
    return *this;
}
