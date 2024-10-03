/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "BootpHandler.h"
#include "Structures.h"
#include "Serializer.h"
#include "IpConverter.h"
#include "Logger.h"

#include <cstdlib> // for system(), remove when arp manipulation is done without calling /sbin/arp

#include <format>
#include <string>
#include <string_view>
#include <unordered_map>

constexpr auto ArpProgram{ "/sbin/arp" };

namespace
{
void addArpEntry(std::string_view iface, std::string_view ip, std::string_view hw)
{
    /*
     * TODO use Linux's own API for doing this instead of std::system.
    */
    auto cmd = std::format("{} -i {} -s {} {}", ArpProgram, iface, ip, hw);
    Log::Debug("Executing: {}", cmd);
    if (int ret = std::system(cmd.c_str()) != 0)
    {
        Log::Critical("Failed to execute ({}): {}", ret, cmd);
    }
}

DHCPMessageType getMessageType(const BOOTP& bootp)
{
    auto it = bootp.options.find(Option_MessageType);
    if (it == bootp.options.end())
        return DHCP_UnknownMessage;

    auto& messageTypeHolder = dynamic_cast<DHCPMessageTypeBOOTPOption&>(*it->second);
    return messageTypeHolder.getMessageType();
}

std::span<const BOOTPOptionKey> getParameterList(const BOOTP& bootp)
{
    auto it = bootp.options.find(Option_ParameterRequestList);
    if (it == bootp.options.end())
        return {};

    auto& parameterListHolder = dynamic_cast<ParameterListBOOTPOption&>(*it->second);
    return parameterListHolder.getParameters();
}

std::uint32_t getRequestedIpAddress(const BOOTP& bootp)
{
    auto it = bootp.options.find(Option_RequestedIp);
    if (it == bootp.options.end())
        return 0;

    auto& ipListHolder = dynamic_cast<IpListBOOTPOption&>(*it->second);
    if (ipListHolder.getIps().empty())
        return 0;

    return ipListHolder.getIps().front();
}

void provideParameterList(const Network& network, const BOOTP& bootp, BOOTP& offer)
{
    /* DHCP Offer */
    auto& offerMessageTypeOption = offer.options[Option_MessageType];
    offerMessageTypeOption = std::make_unique<DHCPMessageTypeBOOTPOption>(DHCP_Offer);

    /*
     * It appears that even though all the following are _options_, they appear to be _required_ to form a "valid" DHCP
     * response. Well-made clients should ask for these in the options request, but for example Sony's PS4 appear to
     * not provide anything useful in the options request and simply assumes these to appear *magically*. So here goes:
    */

    /* Server identifer */
    auto& serverIdentifierOption = offer.options[Option_ServerIdentifier];
    serverIdentifierOption = std::make_unique<IntegerBOOTPOption<std::uint32_t>>(network.getDhcpServerIdentifier());

    /* IP lease duration / time */
    auto& ipLeaseTimeOption = offer.options[Option_IPLeaseTime];
    ipLeaseTimeOption = std::make_unique<IntegerBOOTPOption<std::uint32_t>>(network.getLeaseTime());

    /* Subnet mask */
    std::uint32_t subnetMask = (~0 << (32 - network.getNetworkSize()));
    auto& subnetMaskOption = offer.options[Option_SubnetMask];
    std::vector<std::uint32_t> subnetIpList = { subnetMask };
    subnetMaskOption = std::make_unique<IpListBOOTPOption>(std::move(subnetIpList));

    /* Router's IP */
    auto& routersIpOption = offer.options[Option_Router];
    std::vector<std::uint32_t> routersIpList = {network.getRouterAddress() };
    routersIpOption = std::make_unique<IpListBOOTPOption>(std::move(routersIpList));

    /* DNS servers */
    auto& dnsOption = offer.options[Option_DomainNameServer];
    auto dnsIpList = network.getDnsServers();
    dnsOption = std::make_unique<IpListBOOTPOption>(std::move(dnsIpList));

    /* Broadcast */
    std::uint32_t broadcastAddress = network.getBroadcastAddress();
    auto& broadcastAddressOption = offer.options[Option_BroadcastAddress];
    std::vector<std::uint32_t> broadcastAddressIpList = { broadcastAddress };
    broadcastAddressOption = std::make_unique<IpListBOOTPOption>(std::move(broadcastAddressIpList));

    std::string optionslog;

    auto parameterList = getParameterList(bootp);
    for (auto parameter : parameterList)
    {
        switch (parameter)
        {
            case Option_Pad:
                break; // Don't care.

            case Option_SubnetMask:
                optionslog += "1/SubnetMask, ";
                break;

            case Option_Router:
                optionslog += "3/Routers, ";
                break;

            case Option_DomainNameServer:
                optionslog += "6/DNS, ";
                break;

            case Option_BroadcastAddress:
                optionslog += "28/Broadcast, ";
                break;

            case Option_RequestedIp:
                optionslog += "50/RequestedIp, ";
                break;

            case Option_IPLeaseTime:
                optionslog += "51/IpLeaseTime, ";
                break;

            case Option_ServerIdentifier:
                optionslog += "54/ServerIdentifier, ";
                break;

            case Option_RenewalTime:
            {
                optionslog += "58/RenewalTime, ";
                auto& option = offer.options[Option_RenewalTime];
                option = std::make_unique<IntegerBOOTPOption<std::uint32_t>>(network.getRenewalTime());
                break;
            }

            case Option_RebindingTime:
            {
                optionslog += "59/RebindingTime, ";
                auto& option = offer.options[Option_RebindingTime];
                option = std::make_unique<IntegerBOOTPOption<std::uint32_t>>(network.getRebindingTime());
                break;
            }

            case Option_End:
                break; // Don't care.

            default:
                optionslog += std::format("{}, ", static_cast<int>(parameter));
                break; // unsupported option
        }
    }

    if (optionslog.empty())
    {
        optionslog = "[Empty or unspecified]";
    }
    else
    {
        /* For the comma and space. */
        optionslog.pop_back();
        optionslog.pop_back();
    }

    Log::Debug("Parameter request from {} - {}",
               convertHardwareAddress(bootp.chaddr),
               optionslog);
}

} // anonymous ns

struct BootpHandlerPrivate
{
    explicit BootpHandlerPrivate(std::string deviceName_)
        : deviceName(std::move(deviceName_))
    {
        auto config = Configuration::GetNetworkConfiguration(deviceName);
        auto leases = Configuration::GetPersistentLeasesByInterface(deviceName);
        network.configure(std::move(config), leases);
    }

    std::unordered_map<std::uint64_t, BOOTP> offers;
    Network network;
    std::string deviceName;

    std::optional<BootpResponse> handleDhcpDiscover(const BOOTP& bootp)
    {
        if (bootp.operation != BOOTP_Request)
            return std::nullopt; // This would be a bug in the DHCP client.

        auto address = network.getAvailableAddress(bootp.chaddr);
        if (address == 0)
            return std::nullopt;  // exhausted network, don't offer anything.

        auto& offer = offers[bootp.chaddr];
        offer = bootp;
        offer.operation = BOOTP_Reply;
        offer.options.clear();
        offer.yiaddr = address;

        provideParameterList(network, bootp, offer);

        BootpResponse response;
        response.target = address;
        response.data = serializeBootp(offer);
        if (response.data.empty())
            return std::nullopt; // Logged by serializer.

        Log::Info("Offering address {} to {}",
                   convertIpAddress(address),
                   convertHardwareAddress(bootp.chaddr));

        addArpEntry(deviceName, convertIpAddress(address), convertHardwareAddress(bootp.chaddr));

        offers[bootp.chaddr] = std::move(offer);
        return response;
    }

    std::optional<BootpResponse> handleDhcpRequest(const BOOTP& bootp)
    {
        auto markOfferWithNak = [this] (BOOTP& offer)
        {
            offer.options.clear();
            offer.options[Option_MessageType] = std::make_unique<DHCPMessageTypeBOOTPOption>(DHCP_NAK);
            offer.options[Option_ServerIdentifier] = std::make_unique<IntegerBOOTPOption<std::uint32_t>>(
                    network.getDhcpServerIdentifier());
            offer.yiaddr = 0;
            offer.ciaddr = 0;
        };

        /*
         * No offer was given to this hardware address, check for existing leases.
        */
        if (!offers.contains(bootp.chaddr))
        {
            const auto& lease = network.getLease(bootp.chaddr);

            /*
             * We don't know about this hardware address, send a NAK.
            */
            if (!Network::isLeaseEntryValid(lease))
            {
                Log::Info("Sending NAK to {} because we don't know them", convertHardwareAddress(bootp.chaddr));
                auto nak = bootp;
                markOfferWithNak(nak);
                BootpResponse response;

                // It doesn't make any sense (to me at least) to use any IP address wen NAK'ing in this condition.
                // Should it be the network's broadcast (ie. 192.168.0.255 or the "universal" one, 255.255.255.255 ?)
                // I'm using the network's for now:
                response.target = network.getBroadcastAddress();

                response.data = serializeBootp(nak);
                if (!response.data.empty()) // the opposite condition is logged by serializer.
                {
                    return response;
                }

                return std::nullopt;
            }

            // We know about this hardware address, offer the IP we have in our record.

            offers[bootp.chaddr] = bootp;
            auto& offer = offers[bootp.chaddr];
            offer.operation = BOOTP_Reply;
            offer.yiaddr = lease.ipAddress;
            offer.options.clear();
            provideParameterList(network, bootp, offer);
        }

        auto& offer = offers[bootp.chaddr];
        auto requestedIpAddress = getRequestedIpAddress(bootp);
        auto address = network.getAvailableAddress(bootp.chaddr, requestedIpAddress);

        if (offer.yiaddr != requestedIpAddress || address != requestedIpAddress)
        {
            markOfferWithNak(offer);
            Log::Info("Sending NAK to {} because these aren't equal: yiaddr={}, requested={}, network={}",
                       convertHardwareAddress(bootp.chaddr),
                       convertIpAddress(offer.yiaddr),
                       convertIpAddress(requestedIpAddress),
                       convertIpAddress(address));
        }
        else
        {
            if (network.reserveAddress(bootp.chaddr, address))
            {
                offer.options[Option_MessageType] = std::make_unique<DHCPMessageTypeBOOTPOption>(DHCP_ACK);
                Log::Info("Sending ACK on address {} to {}",
                           convertIpAddress(address),
                           convertHardwareAddress(bootp.chaddr));
            }
            else
            {
                markOfferWithNak(offer);
                Log::Info("Sending NAK to {} because address reservation of {} failed (exhausted network or requested address is illegal)",
                           convertHardwareAddress(bootp.chaddr),
                           convertIpAddress(address));
            }
        }

        BootpResponse response;
        response.target = address;
        response.data = serializeBootp(offer);

        offers.erase(bootp.chaddr);

        if (!response.data.empty()) // the opposite condition is logged by serializer.
        {
            return response;
        }

        return std::nullopt;
    }

    void handleDhcpRelease(const BOOTP& bootp)
    {
        Log::Info("Releasing address {} from {}", convertIpAddress(bootp.ciaddr), convertHardwareAddress(bootp.chaddr));
        network.releaseAddress(bootp.ciaddr);
    }

    std::optional<BootpResponse> handleRequest(const BOOTP& bootp)
    {
        auto messageType = getMessageType(bootp);
        switch (messageType)
        {
            case DHCP_Discover:
                Log::Info("Handling DHCP Discover from {}", convertHardwareAddress(bootp.chaddr));
                return handleDhcpDiscover(bootp);

            case DHCP_Request:
                Log::Info("Handling DHCP Request from {}", convertHardwareAddress(bootp.chaddr));
                return handleDhcpRequest(bootp);

            case DHCP_Release:
                Log::Info("Handling DHCP Release from {}", convertHardwareAddress(bootp.chaddr));
                handleDhcpRelease(bootp);
                break;

            case DHCP_Decline:
                Log::Info("Handling DHCP Decline (as a release) from {}", convertHardwareAddress(bootp.chaddr));
                // TODO in fact, reserve the address internally as it's most likely unusable anyway.
                handleDhcpRelease(bootp);
                break;

            default:
                break; // don't care.
        }

        return std::nullopt;
    }
}; // BootpHandlerPrivate

BootpHandler::BootpHandler(std::string deviceName)
{
    mp = std::make_unique<BootpHandlerPrivate>(std::move(deviceName));
}

BootpHandler::~BootpHandler() = default;

std::optional<BootpResponse> BootpHandler::handleRequest(std::span<const std::uint8_t> data)
{
    BOOTP request;
    if (!deserializeBootp(data, request))
    {
        Log::Warning("Failed to deserialize BOOTP message");
        return std::nullopt;
    }

    return mp->handleRequest(request);
}
