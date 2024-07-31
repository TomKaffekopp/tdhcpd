/*
 * TDHCPD Lease Viewer - View lease files made by TDHCPD.
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "IpConverter.h"
#include "Structures.h"
#include "Configuration.h"

#include <ctime>

#include <iostream>
#include <string>

void print(std::string_view format, auto&&...args)
{
    std::cout << std::vformat(format, std::make_format_args(args...));
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        print("Usage: {} <filename>\n", argv[0]);
        return 1;
    }

    std::string filename(argv[1]);
    auto leases = Configuration::GetPersistentLeasesByFile(filename);

    for (const auto& lease : leases)
    {
        std::string leaseStart(std::ctime(&lease.startTime));

        auto hwAddress = convertHardwareAddress(lease.hwAddress);
        auto ipAddress = convertIpAddress(lease.ipAddress);

        print("Lease start        {}", leaseStart); // ctime() adds a \n on the end
        print("Hardware address   {}\n", hwAddress);
        print("IPv4 address       {}\n\n", ipAddress);
    }

    print("Total amount of leases: {}\n", leases.size());

    return 0;
}
