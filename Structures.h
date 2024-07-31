/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include <ctime>
#include <cstdint>

#include <unordered_map>
#include <memory>
#include <vector>
#include <span>

enum BOOTPOperation : std::uint8_t
{
    BOOTP_Request = 1,
    BOOTP_Reply   = 2
};

enum BOOTPOptionKey : std::uint8_t
{
    Option_Pad                  = 0,

    Option_SubnetMask           = 1,
    Option_Router               = 3,
    Option_DomainNameServer     = 6,
    Option_BroadcastAddress     = 28,
    Option_RequestedIp          = 50,
    Option_IPLeaseTime          = 51,
    Option_MessageType          = 53,
    Option_ServerIdentifier     = 54,
    Option_ParameterRequestList = 55,

    Option_End                  = 255
};

enum DHCPMessageType : std::uint8_t
{
    DHCP_UnknownMessage = 0, /* not part of the spec, used for error detection. */

    DHCP_Discover   = 1,
    DHCP_Offer      = 2,
    DHCP_Request    = 3,
    DHCP_Decline    = 4,
    DHCP_ACK        = 5,
    DHCP_NAK        = 6,
    DHCP_Release    = 7
};

struct Lease
{
    std::time_t startTime{};
    std::uint64_t hwAddress{};
    std::uint32_t ipAddress{};
};

constexpr auto StartTimeLen = sizeof(Lease::startTime);
constexpr auto HwAddressLen = sizeof(Lease::hwAddress);
constexpr auto IpAddressLen = sizeof(Lease::ipAddress);
constexpr auto LeaseLen = StartTimeLen + HwAddressLen + IpAddressLen;

class BOOTPOption
{
public:
    virtual ~BOOTPOption() = default;
    virtual std::vector<std::uint8_t> serialize() = 0;
};

class ParameterListBOOTPOption : public BOOTPOption
{
    std::vector<BOOTPOptionKey> m_parameters;

public:
    explicit ParameterListBOOTPOption(std::vector<BOOTPOptionKey>&& parameters)
        : m_parameters(std::move(parameters))
    {}

    explicit ParameterListBOOTPOption(std::span<std::uint8_t> data)
    {
        if (data.empty())
        {
            return; // Is this an error?
        }

        // TODO should checks be done here for what we support?

        for (auto i = 1; i < data.front() + 1; ++i)
            m_parameters.emplace_back(static_cast<BOOTPOptionKey>(data[i]));
    }

    std::vector<std::uint8_t> serialize() override
    {
        std::vector<std::uint8_t> data = { static_cast<std::uint8_t>(m_parameters.size()) };

        for (auto param : m_parameters)
        {
            data.emplace_back(param);
        }

        return data;
    }

    [[nodiscard]]
    std::span<const BOOTPOptionKey> getParameters() const
    {
        return m_parameters;
    }
};

class DHCPMessageTypeBOOTPOption : public BOOTPOption
{
    DHCPMessageType m_messageType;

public:
    explicit DHCPMessageTypeBOOTPOption(DHCPMessageType messageType)
        : m_messageType(messageType)
    {}

    explicit DHCPMessageTypeBOOTPOption(std::span<std::uint8_t> data)
    {
        if (data.empty() || data.front() == 0 /* empty payload */)
        {
            // This will cause us to not send any response
            m_messageType = DHCP_UnknownMessage;
        }
        else
        {
            // TODO this should be sanitized.
            m_messageType = static_cast<DHCPMessageType>(data[1]);
        }
    }

    std::vector<std::uint8_t> serialize() override
    {
        std::vector<std::uint8_t> data = { sizeof(DHCPMessageType), m_messageType };
        return data;
    }

    [[nodiscard]]
    DHCPMessageType getMessageType() const { return m_messageType; }
};

class IpListBOOTPOption : public BOOTPOption
{
    std::vector<std::uint32_t> m_ips;

public:
    explicit IpListBOOTPOption(std::vector<std::uint32_t>&& ips)
        : m_ips(std::move(ips))
    {}

    explicit IpListBOOTPOption(std::span<std::uint8_t> data)
    {
        if (data.empty())
        {
            return; // Is this an error?
        }

        const auto count = data.front();

        for (int i = 0; i < count; ++i)
        {
            std::uint32_t addr{};
            for (int j = 1; j <= 4; ++j)
            {
                addr <<= 8;
                addr |= data[j + i * 4];
            }
            m_ips.emplace_back(addr);
        }
    }

    std::vector<std::uint8_t> serialize() override
    {
        std::vector<std::uint8_t> data = { static_cast<std::uint8_t>(m_ips.size() * 4) };
        for (auto ip : m_ips)
        {
            data.emplace_back(static_cast<std::uint8_t>((ip & 0xFF000000) >> 24));
            data.emplace_back(static_cast<std::uint8_t>((ip & 0x00FF0000) >> 16));
            data.emplace_back(static_cast<std::uint8_t>((ip & 0x0000FF00) >> 8));
            data.emplace_back(static_cast<std::uint8_t>( ip & 0x000000FF));
        }
        return data;
    }

    [[nodiscard]]
    std::span<const std::uint32_t> getIps() const { return m_ips; }
};

template<typename T>
class IntegerBOOTPOption : public BOOTPOption
{
    T m_value{};

public:
    explicit IntegerBOOTPOption(T value)
        : m_value(value)
    {}

    explicit IntegerBOOTPOption(std::span<std::uint8_t> data)
    {
        if (data.empty())
        {
            return; // Is this an error?
        }

        // TODO if data.front() is not equal to sizeof(T), yield a warning in log.

        for (int i = 0; i < data.front(); ++i)
        {
            if constexpr (sizeof(T) > 1)
                m_value <<= 8;
            m_value |= data[i + 1];
        }
    }

    std::vector<std::uint8_t> serialize() override
    {
        std::vector<std::uint8_t> data = { static_cast<std::uint8_t>(sizeof(T)) };

        for (auto i = 0u; i < sizeof(T); ++i)
        {
            const auto shift = sizeof(T) * 8 - (i + 1) * 8;
            const T mask = static_cast<T>(0xFF) << shift;
            data.emplace_back( (m_value & mask) >> shift );
        }

        return data;
    }

    [[nodiscard]]
    T getValue() const { return m_value; }
};

struct BOOTP
{
    /*
     * NOTE!
     * When making a copy of BOOTP, the "options" will be lost!
     * CBA to fix that bit, and it doesn't really matter right now anyway.
     * This is because the unordered_map of options holds a unique_ptr, which is used for polymorphism.
    */

    BOOTP() = default;
    ~BOOTP() = default;
    BOOTP(const BOOTP&);
    BOOTP(BOOTP&&) noexcept;

    BOOTP& operator=(const BOOTP&);
    BOOTP& operator=(BOOTP&&) noexcept;

    BOOTPOperation operation{ BOOTP_Reply };
    std::uint8_t hardwareType{ 0x01 }; /* ethernet: 0x01 */
    std::uint8_t hardwareAddressLength{ 6 }; /* For MAC address, 6 bytes. */
    std::uint8_t hops{};
    std::uint32_t transactionId{};
    std::uint16_t secondsElapsed{}; /* Not used by DHCP it seems (?) - this is seconds since first BOOTREQUEST message. */
    std::uint16_t flags{};
    std::uint32_t ciaddr{};
    std::uint32_t yiaddr{};
    std::uint32_t siaddr{};
    std::uint32_t giaddr{};
    std::uint64_t chaddr{}; // Hardware address (MAC) stored here
    /* 64 + 128 bytes of unused data, padded with 0 when sending this structure. */
    std::uint32_t magic{ 0x63825363 };
    std::unordered_map<BOOTPOptionKey, std::unique_ptr<BOOTPOption>> options;
};
