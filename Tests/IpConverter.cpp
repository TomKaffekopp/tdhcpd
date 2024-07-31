/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "IpConverter.h"

#include <gtest/gtest.h>

TEST(IpConverter, Concatenate)
{
    auto addr = concatenateIpAddress(192, 168, 1, 23);
    // 0xC0A80117 : 192.168.1.23
    EXPECT_EQ(0xC0A80117, addr);
}

TEST(IpConverter, StringToIntegerConvert)
{
    auto addr = convertIpAddress("192.168.1.23");
    // 0xC0A80117 : 192.168.1.23
    EXPECT_EQ(0xC0A80117, addr);
}

TEST(IpConverter, IntegerToStringConvert)
{
    auto addr = convertIpAddress(0xC0A80117);
    // 0xC0A80117 : 192.168.1.23
    EXPECT_EQ("192.168.1.23", addr);
}
