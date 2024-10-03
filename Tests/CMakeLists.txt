# TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
# Copyright (C) 2024  Tom-Andre Barstad.
# This software is licensed under the Software Attribution License.
# See LICENSE for more information.

cmake_minimum_required(VERSION 3.24)

project(DhcpdTests)

enable_testing()
find_package(GTest REQUIRED)

add_executable(${PROJECT_NAME}
    Structures.cpp
    IpConverter.cpp
    Serializer.cpp
    Network.cpp
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    GTest::gtest
    GTest::gtest_main
    ${NetworkLib}
    ${ConfigurationLib}
    ${IpConverterLib}
    ${SerializerLib}
    ${LoggerLib}
)
