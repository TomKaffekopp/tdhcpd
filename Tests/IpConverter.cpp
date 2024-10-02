/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "IpConverter.h"

#include <gtest/gtest.h>

TEST(IpConverter, ConcatenateIpAddress)
{
    auto addr = concatenateIpAddress(192, 168, 1, 23);
    // 0xC0A80117 : 192.168.1.23
    EXPECT_EQ(0xC0A80117, addr);
}

TEST(IpConverter, ConcatenateHardwareAddress)
{
    auto addr = concatenateHardwareAddress(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    EXPECT_EQ(0xAABBCCDDEEFF, addr);
}

TEST(IpConverter, StringToIntegerConvert)
{
    bool ok{};
    auto addr = convertIpAddress("192.168.1.23", ok);
    // 0xC0A80117 : 192.168.1.23
    EXPECT_TRUE(ok);
    EXPECT_EQ(0xC0A80117, addr);
}

TEST(IpConverter, IntegerToStringConvert)
{
    auto addr = convertIpAddress(0xC0A80117);
    // 0xC0A80117 : 192.168.1.23
    EXPECT_EQ("192.168.1.23", addr);
}
