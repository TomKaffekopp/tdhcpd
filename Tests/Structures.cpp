/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "Structures.h"
#include "IpConverter.h"

#include <gtest/gtest.h>

TEST(BOOTPOptions, ParameterList_Serialize)
{
    std::vector<BOOTPOptionKey> parameters = {
        Option_SubnetMask,
        Option_DomainNameServer,
        Option_IPLeaseTime,
    };

    ParameterListBOOTPOption option(std::move(parameters));
    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(4, data.size());

    EXPECT_EQ(3, data[0]);
    EXPECT_EQ(Option_SubnetMask, data[1]);
    EXPECT_EQ(Option_DomainNameServer, data[2]);
    EXPECT_EQ(Option_IPLeaseTime, data[3]);
}

TEST(BOOTPOptions, ParameterList_Deserialize)
{
    std::vector<std::uint8_t> data = { 0x03, 0x01, 0x06, 0x33 };
    ParameterListBOOTPOption option(data);

    auto parameters = option.getParameters();
    ASSERT_EQ(3, parameters.size());

    EXPECT_EQ(Option_SubnetMask, parameters[0]);
    EXPECT_EQ(Option_DomainNameServer, parameters[1]);
    EXPECT_EQ(Option_IPLeaseTime, parameters[2]);
}

TEST(BOOTPOptions, DHCPMessageType_Serialize)
{
    DHCPMessageTypeBOOTPOption option(DHCP_Discover);
    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(2, data.size());

    EXPECT_EQ(1, data[0]);
    EXPECT_EQ(DHCP_Discover, data[1]);
}

TEST(BOOTPOptions, DHCPMessageType_Deserialize)
{
    std::vector<std::uint8_t> data = { 1, DHCP_Discover };
    DHCPMessageTypeBOOTPOption option(data);

    EXPECT_EQ(DHCP_Discover, option.getMessageType());
}

TEST(BOOTPOptions, IpList_Serialize)
{
    std::vector<std::uint32_t> ips = {
        concatenateIpAddress(192, 168, 1, 23),
        concatenateIpAddress(255, 255, 255, 255),
        concatenateIpAddress(0, 0, 0, 0)
    };

    IpListBOOTPOption option(std::move(ips));

    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(13, data.size());

    EXPECT_EQ(12, data[0]);

    auto addr1 = convertIpAddress(concatenateIpAddress(data[1], data[2],  data[3],  data[4]));
    auto addr2 = convertIpAddress(concatenateIpAddress(data[5], data[6],  data[7],  data[8]));
    auto addr3 = convertIpAddress(concatenateIpAddress(data[9], data[10], data[11], data[12]));

    EXPECT_EQ("192.168.1.23", addr1);
    EXPECT_EQ("255.255.255.255", addr2);
    EXPECT_EQ("0.0.0.0", addr3);
}

TEST(BOOTPOptions, IpList_Deserialize)
{
    std::vector<std::uint8_t> data = { 0x0C, 0xC0, 0xA8, 0x01, 0x17, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };
    IpListBOOTPOption option(data);

    auto ips = option.getIps();

    ASSERT_FALSE(ips.empty());

    EXPECT_EQ("192.168.1.23", convertIpAddress(ips[0]));
    EXPECT_EQ("255.255.255.255", convertIpAddress(ips[1]));
    EXPECT_EQ("0.0.0.0", convertIpAddress(ips[2]));
}

TEST(BOOTPOptions, Integer_8_Serialize)
{
    std::uint8_t value = 0xAB;
    IntegerBOOTPOption option(value);

    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(1, data.front());

    EXPECT_EQ(0xAB, data[1]);
}

TEST(BOOTPOptions, Integer_8_Deserialize)
{
    std::vector<std::uint8_t> data = { 0x01, 0xAB };
    IntegerBOOTPOption<std::uint8_t> option(data);

    EXPECT_EQ(0xAB, option.getValue());
}

TEST(BOOTPOptions, Integer_16_Serialize)
{
    std::uint16_t value = 0xABCD;
    IntegerBOOTPOption option(value);

    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(2, data.front());

    EXPECT_EQ(0xAB, data[1]);
    EXPECT_EQ(0xCD, data[2]);
}

TEST(BOOTPOptions, Integer_16_Deserialize)
{
    std::vector<std::uint8_t> data = { 0x02, 0xAB, 0xCD };
    IntegerBOOTPOption<std::uint16_t> option(data);

    EXPECT_EQ(0xABCD, option.getValue());
}

TEST(BOOTPOptions, Integer_32_Serialize)
{
    std::uint32_t value = 0xABC12DEF;
    IntegerBOOTPOption option(value);

    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(4, data.front());

    EXPECT_EQ(0xAB, data[1]);
    EXPECT_EQ(0xC1, data[2]);
    EXPECT_EQ(0x2D, data[3]);
    EXPECT_EQ(0xEF, data[4]);
}

TEST(BOOTPOptions, Integer_32_Deserialize)
{
    std::vector<std::uint8_t> data = { 0x04, 0xAB, 0xC1, 0x2D, 0xEF };
    IntegerBOOTPOption<std::uint32_t> option(data);

    EXPECT_EQ(0xABC12DEF, option.getValue());
}

TEST(BOOTPOptions, Integer_64_Serialize)
{
    std::uint64_t value = 0xABC12DEFCBA34FED;
    IntegerBOOTPOption<unsigned long long> option(value);

    auto data = option.serialize();

    ASSERT_FALSE(data.empty());
    ASSERT_EQ(8, data.front());

    EXPECT_EQ(0xAB, data[1]);
    EXPECT_EQ(0xC1, data[2]);
    EXPECT_EQ(0x2D, data[3]);
    EXPECT_EQ(0xEF, data[4]);
    EXPECT_EQ(0xCB, data[5]);
    EXPECT_EQ(0xA3, data[6]);
    EXPECT_EQ(0x4F, data[7]);
    EXPECT_EQ(0xED, data[8]);
}

TEST(BOOTPOptions, Integer_64_Deserialize)
{
    std::vector<std::uint8_t> data = { 0x08, 0xAB, 0xC1, 0x2D, 0xEF, 0xCB, 0xA3, 0x4F, 0xED };
    IntegerBOOTPOption<std::uint64_t> option(data);

    EXPECT_EQ(0xABC12DEFCBA34FED, option.getValue());
}
