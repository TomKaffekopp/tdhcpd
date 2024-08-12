/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "Network.h"
#include "IpConverter.h"

#include <gtest/gtest.h>

#include <thread>
#include <chrono>

TEST(AddressPoolTests, AvailableAddress)
{
    Network net;

    auto adr1 = net.getAvailableAddress(0);
    net.reserveAddress(0, adr1);

    auto adr2 = net.getAvailableAddress(1);
    net.reserveAddress(1, adr2);

    auto adr3 = net.getAvailableAddress(2);
    net.reserveAddress(2, adr3);

    net.releaseAddress(adr2);

    auto adr4 = net.getAvailableAddress(3);

    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 100), adr1);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 101), adr2);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 102), adr3);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 101), adr4);
}

TEST(AddressPoolTests, PreferredAddress)
{
    Network net;

    auto adr1 = net.getAvailableAddress(10);
    net.reserveAddress(10, adr1);

    auto adr2 = net.getAvailableAddress(11);
    net.reserveAddress(11, adr2);

    auto adr3 = net.getAvailableAddress(12);
    net.reserveAddress(12, adr3);

    auto adr4 = net.getAvailableAddress(13);
    net.reserveAddress(13, adr4);

    auto adr5 = net.getAvailableAddress(14);
    net.reserveAddress(14, adr5);

    net.releaseAddress(adr3);
    net.releaseAddress(adr4);


    auto adr6 = net.getAvailableAddress(15, concatenateIpAddress(192, 168, 200, 103));
    net.reserveAddress(15, adr6);

    auto adr7 = net.getAvailableAddress(16);
    net.reserveAddress(15, adr7);

    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 100), adr1);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 101), adr2);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 102), adr3);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 103), adr4);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 104), adr5);

    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 103), adr6);
    EXPECT_EQ(concatenateIpAddress(192, 168, 200, 102), adr7);
}

TEST(AddressPoolTests, ReuseFromSameHardwareAddress)
{
    Network net;

    auto adr1 = net.getAvailableAddress(100);
    net.reserveAddress(100, adr1);

    auto adr2 = net.getAvailableAddress(100);
    net.reserveAddress(100, adr2);

    EXPECT_EQ(adr1, adr2);
}

TEST(AddressPoolTests, PreferredFromDifferentNetwork_1)
{
    Network net;

    const auto preferred = concatenateIpAddress(10, 0, 0, 10);
    const auto actual = concatenateIpAddress(192, 168, 200, 100);

    auto adr1 = net.getAvailableAddress(100, preferred);
    EXPECT_EQ(actual, adr1);

    auto ok = net.reserveAddress(100, preferred);
    EXPECT_FALSE(ok);
}

TEST(AddressPoolTests, PreferredFromDifferentNetwork_2)
{
    Network net;

    const auto preferred = concatenateIpAddress(192, 168, 1, 2);
    const auto actual = concatenateIpAddress(192, 168, 200, 100);

    auto adr1 = net.getAvailableAddress(100, preferred);
    EXPECT_EQ(actual, adr1);

    auto ok = net.reserveAddress(100, preferred);
    EXPECT_FALSE(ok);
}

TEST(AddressPoolTests, LeaseTime_WithoutPreferred_DifferentHardware)
{
    /* Tests with different hardware addresses, without preferred address */

    Network net;
    net.setLeaseDuration(0);

    auto adr1 = net.getAvailableAddress(200);
    net.reserveAddress(200, adr1);

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto adr2 = net.getAvailableAddress(201);
    net.reserveAddress(201, adr2);

    EXPECT_EQ(adr1, adr2);
}

TEST(AddressPoolTests, LeaseTime_WithoutPreferred_SameHardware)
{
    /* Tests with same hardware address, with preferred address */

    Network net;
    net.setLeaseDuration(0);

    auto adr1 = net.getAvailableAddress(300);
    net.reserveAddress(300, adr1);

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto adr2 = net.getAvailableAddress(300);
    net.reserveAddress(300, adr2);

    EXPECT_EQ(adr1, adr2);
}

TEST(AddressPoolTests, LeaseTime_WithPreferred_DifferentHardware)
{
    /* Tests with different hardware addresses, with preferred address */

    Network net;
    net.setLeaseDuration(0);

    const auto preferred = concatenateIpAddress(192, 168, 200, 123);

    auto adr1 = net.getAvailableAddress(200, preferred);
    EXPECT_EQ(preferred, adr1);

    net.reserveAddress(200, adr1);

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto adr2 = net.getAvailableAddress(201, preferred);
    EXPECT_EQ(preferred, adr2);

    net.reserveAddress(201, adr2);
}

TEST(AddressPoolTests, LeaseTime_WithPreferred_SameHardware)
{
    /* Tests with same hardware address, with preferred address */

    Network net;
    net.setLeaseDuration(0);

    const auto preferred = concatenateIpAddress(192, 168, 200, 123);

    auto adr1 = net.getAvailableAddress(300, preferred);
    EXPECT_EQ(preferred, adr1);

    net.reserveAddress(300, adr1);

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto adr2 = net.getAvailableAddress(300, preferred);
    EXPECT_EQ(preferred, adr1);

    net.reserveAddress(300, adr2);
}
