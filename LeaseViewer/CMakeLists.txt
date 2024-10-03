# TDHCPD Lease Viewer - View lease files made by TDHCPD.
# Copyright (C) 2024  Tom-Andre Barstad.
# This software is licensed under the Software Attribution License.
# See LICENSE for more information.

cmake_minimum_required(VERSION 3.28)
project(leaseviewer)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${ConfigurationLib}
    ${IpConverterLib}
    ${LoggerLib}
)
