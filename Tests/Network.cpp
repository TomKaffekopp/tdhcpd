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

TEST(AddressPoolTests, SmallNetwork_30)
{
    Network net;

    /*
     * Network: 192.168.123.108/30
     * Usable addresses:
     * - 192.168.123.109 (router's)
     * - 192.168.123.110 (dhcp)
     * Broadcast: 192.168.123.111
    */

    net.setNetworkSpace(concatenateIpAddress(192, 168, 123, 108));
    net.setNetworkSize(30);

    net.setRouterAddress(concatenateIpAddress(192, 168, 123, 109));
    net.setDhcpServerIdentifier(concatenateIpAddress(192, 168, 123, 109));

    net.setDhcpRange(concatenateIpAddress(192, 168, 123, 110), concatenateIpAddress(192, 168, 123, 110));

    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 111), net.getBroadcastAddress());

    auto ip = net.getAvailableAddress(100);
    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 110), ip);

    auto ok = net.reserveAddress(100, ip);
    EXPECT_TRUE(ok);

    auto ip2 = net.getAvailableAddress(101);
    EXPECT_EQ(0, ip2); // pool exhausted
}

TEST(AddressPoolTests, SmallNetwork_29)
{
    Network net;

    /*
     * Network: 192.168.123.112/29
     * Usable addresses:
     * - 192.168.123.113 (router's)
     * - 192.168.123.114..118 (dhcp)
     * Broadcast: 192.168.123.119
    */

    net.setNetworkSpace(concatenateIpAddress(192, 168, 123, 112));
    net.setNetworkSize(29);

    net.setRouterAddress(concatenateIpAddress(192, 168, 123, 113));
    net.setDhcpServerIdentifier(concatenateIpAddress(192, 168, 123, 113));

    net.setDhcpRange(concatenateIpAddress(192, 168, 123, 114), concatenateIpAddress(192, 168, 123, 118));

    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 119), net.getBroadcastAddress());


    /* reserve 114 */
    auto ip_1 = net.getAvailableAddress(100);
    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 114), ip_1);

    auto ok_1 = net.reserveAddress(100, ip_1);
    EXPECT_TRUE(ok_1);


    /* reserve 115 */
    auto ip_2 = net.getAvailableAddress(101);
    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 115), ip_2);

    auto ok_2 = net.reserveAddress(101, ip_2);
    EXPECT_TRUE(ok_2);


    /* reserve 116 */
    auto ip_3 = net.getAvailableAddress(102);
    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 116), ip_3);

    auto ok_3 = net.reserveAddress(102, ip_3);
    EXPECT_TRUE(ok_3);


    /* reserve 117 */
    auto ip_4 = net.getAvailableAddress(104);
    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 117), ip_4);

    auto ok_4 = net.reserveAddress(103, ip_4);
    EXPECT_TRUE(ok_4);


    /* reserve 118 */
    auto ip_5 = net.getAvailableAddress(104);
    EXPECT_EQ(concatenateIpAddress(192, 168, 123, 118), ip_5);

    auto ok_5 = net.reserveAddress(104, ip_5);
    EXPECT_TRUE(ok_5);


    /* exhausted */
    auto ip_6 = net.getAvailableAddress(105);
    EXPECT_EQ(0, ip_6);
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
