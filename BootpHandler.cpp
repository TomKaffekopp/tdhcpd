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
#include <queue>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <string_view>

constexpr auto ArpProgram{ "/sbin/arp" };

namespace
{
struct Request
{
    std::string deviceSource;
    BOOTP bootp;
};

std::queue<Request> requests;
std::queue<BootpResponse> outbound;
std::unordered_map<std::uint64_t, BOOTP> offers;

std::mutex requestsMutex;
std::mutex responsesMutex;

std::thread thread;
std::atomic_bool running{ true };

std::unordered_map<std::string, Network> networks;

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

bool hasRequests()
{
    std::lock_guard lockGuard(requestsMutex);
    return !requests.empty();
}

Request getNextRequest()
{
    std::lock_guard lockGuard(requestsMutex);
    auto request = std::move(requests.front());
    requests.pop();
    return request;
}

DHCPMessageType getMessageType(const Request& request)
{
    auto it = request.bootp.options.find(Option_MessageType);
    if (it == request.bootp.options.end())
        return DHCP_UnknownMessage;

    auto& messageTypeHolder = dynamic_cast<DHCPMessageTypeBOOTPOption&>(*it->second);
    return messageTypeHolder.getMessageType();
}

std::span<const BOOTPOptionKey> getParameterList(const Request& request)
{
    auto it = request.bootp.options.find(Option_ParameterRequestList);
    if (it == request.bootp.options.end())
        return {};

    auto& parameterListHolder = dynamic_cast<ParameterListBOOTPOption&>(*it->second);
    return parameterListHolder.getParameters();
}

std::uint32_t getRequestedIpAddress(const Request& request)
{
    auto it = request.bootp.options.find(Option_RequestedIp);
    if (it == request.bootp.options.end())
        return 0;

    auto& ipListHolder = dynamic_cast<IpListBOOTPOption&>(*it->second);
    if (ipListHolder.getIps().empty())
        return 0;

    return ipListHolder.getIps().front();
}

void provideParameterList(const Network& network, const Request& request, BOOTP& offer)
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

    auto parameterList = getParameterList(request);
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
               convertHardwareAddress(request.bootp.chaddr),
               optionslog);
}

void handleDhcpDiscover(const Request& request)
{
    if (request.bootp.operation != BOOTP_Request)
        return; // This would be a bug in the DHCP client.

    auto& network = networks[request.deviceSource];
    auto address = network.getAvailableAddress(request.bootp.chaddr);
    if (address == 0)
        return;  // exhausted network, don't offer anything.

    auto& offer = offers[request.bootp.chaddr];
    offer = request.bootp;
    offer.operation = BOOTP_Reply;
    offer.options.clear();
    offer.yiaddr = address;

    provideParameterList(network, request, offer);

    BootpResponse response;
    response.target = address;
    response.data = serializeBootp(offer);
    if (response.data.empty())
        return; // Logged by serializer.

    Log::Info("Offering address {} to {}",
               convertIpAddress(address),
               convertHardwareAddress(request.bootp.chaddr));

    addArpEntry(request.deviceSource, convertIpAddress(address), convertHardwareAddress(request.bootp.chaddr));

    outbound.emplace(std::move(response));
    offers[request.bootp.chaddr] = std::move(offer);
}

void handleDhcpRequest(const Request& request)
{
    auto& network = networks[request.deviceSource];
    auto markOfferWithNak = [&network] (BOOTP& offer)
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
    if (!offers.contains(request.bootp.chaddr))
    {
        const auto& lease = network.getLease(request.bootp.chaddr);

        /*
         * We don't know about this hardware address, send a NAK.
        */
        if (!Network::isLeaseEntryValid(lease))
        {
            Log::Info("Sending NAK to {} because we don't know them", convertHardwareAddress(request.bootp.chaddr));
            auto nak = request.bootp;
            markOfferWithNak(nak);
            BootpResponse response;

            // It doesn't make any sense (to me at least) to use any IP address wen NAK'ing in this condition.
            // Should it be the network's broadcast (ie. 192.168.0.255 or the "universal" one, 255.255.255.255 ?)
            // I'm using the network's for now:
            response.target = network.getBroadcastAddress();

            response.data = serializeBootp(nak);
            if (!response.data.empty()) // the opposite condition is logged by serializer.
            {
                outbound.emplace(std::move(response));
            }
            return;
        }

        // We know about this hardware address, offer the IP we have in our record.

        offers[request.bootp.chaddr] = request.bootp;
        auto& offer = offers[request.bootp.chaddr];
        offer.operation = BOOTP_Reply;
        offer.yiaddr = lease.ipAddress;
        offer.options.clear();
        provideParameterList(network, request, offer);
    }

    auto& offer = offers[request.bootp.chaddr];
    auto requestedIpAddress = getRequestedIpAddress(request);
    auto address = network.getAvailableAddress(request.bootp.chaddr, requestedIpAddress);

    if (offer.yiaddr != requestedIpAddress || address != requestedIpAddress)
    {
        markOfferWithNak(offer);
        Log::Info("Sending NAK to {} because these aren't equal: yiaddr={}, requested={}, network={}",
                   convertHardwareAddress(request.bootp.chaddr),
                   convertIpAddress(offer.yiaddr),
                   convertIpAddress(requestedIpAddress),
                   convertIpAddress(address));
    }
    else
    {
        if (network.reserveAddress(request.bootp.chaddr, address))
        {
            offer.options[Option_MessageType] = std::make_unique<DHCPMessageTypeBOOTPOption>(DHCP_ACK);
            Log::Info("Sending ACK on address {} to {}",
                       convertIpAddress(address),
                       convertHardwareAddress(request.bootp.chaddr));

            // TODO this should be done by the Network class.
            Configuration::SavePersistentLeases(network.getAllLeases(), network.getLeaseFile());
        }
        else
        {
            markOfferWithNak(offer);
            Log::Info("Sending NAK to {} because address reservation of {} failed (exhausted network or requested address is illegal)",
                       convertHardwareAddress(request.bootp.chaddr),
                       convertIpAddress(address));
        }
    }

    BootpResponse response;
    response.target = address;
    response.data = serializeBootp(offer);
    if (!response.data.empty()) // the opposite condition is logged by serializer.
    {
        outbound.emplace(std::move(response));
    }

    offers.erase(request.bootp.chaddr);
}

void handleDhcpRelease(const Request& request)
{
    Log::Info("Releasing address {} from {}", convertIpAddress(request.bootp.ciaddr), convertHardwareAddress(request.bootp.chaddr));
    auto& network = networks[request.deviceSource];
    network.releaseAddress(request.bootp.ciaddr);
}

void handleRequest(Request&& request)
{
    auto messageType = getMessageType(request);
    switch (messageType)
    {
        case DHCP_Discover:
            Log::Info("Handling DHCP Discover from {}", convertHardwareAddress(request.bootp.chaddr));
            handleDhcpDiscover(request);
            break;

        case DHCP_Request:
            Log::Info("Handling DHCP Request from {}", convertHardwareAddress(request.bootp.chaddr));
            handleDhcpRequest(request);
            break;

        case DHCP_Release:
            Log::Info("Handling DHCP Release from {}", convertHardwareAddress(request.bootp.chaddr));
            handleDhcpRelease(request);
            break;

        case DHCP_Decline:
            Log::Info("Handling DHCP Decline (as a release) from {}", convertHardwareAddress(request.bootp.chaddr));
            // TODO in fact, reserve the address internally as it's most likely unusable anyway.
            handleDhcpRelease(request);
            break;

        default:
            break; // don't care.
    }
}

void handlerThread()
{
    Log::Info("Started Bootp handler thread");
    while (running)
    {
        if (!hasRequests())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto request = getNextRequest();
        handleRequest(std::move(request));
    }
}

} // anonymous ns

void BootpHandler::start(std::unordered_map<std::string, Network>&& networks_)
{
    networks = std::move(networks_);
    thread = std::thread(handlerThread);
}

void BootpHandler::stop()
{
    running = false;
    if (thread.joinable())
        thread.join();
}

void BootpHandler::addRequestData(std::string deviceSource, std::span<const std::uint8_t> data)
{
    std::lock_guard lockGuard(requestsMutex);

    Request request;
    if (!deserializeBootp(data, request.bootp))
    {
        Log::Warning("Failed to deserialize BOOTP message");
        return;
    }

    request.deviceSource = std::move(deviceSource);
    requests.emplace(std::move(request));
}

std::optional<BootpResponse> BootpHandler::getNextResponse()
{
    std::lock_guard lockGuard(responsesMutex);
    if (outbound.empty())
        return std::nullopt;

    auto response = std::move(outbound.front()); // not sure if this allows the element to be, in fact, moved.
    outbound.pop();

    return response;
}
