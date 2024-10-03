# TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
# Copyright (C) 2024  Tom-Andre Barstad.
# This software is licensed under the Software Attribution License.
# See LICENSE for more information.

cmake_minimum_required(VERSION 3.28)
project(tdhcpd)

set(TDHCPD_VERSION_NUMBER "1.1.0")
set(TDHCPD_VERSION_SUFFIX "dev5")

if ("${TDHCPD_VERSION_SUFFIX}" STREQUAL "")
    set(TDHCPD_VERSION "${TDHCPD_VERSION_NUMBER}")
else()
    set(TDHCPD_VERSION "${TDHCPD_VERSION_NUMBER}-${TDHCPD_VERSION_SUFFIX}")
endif()

include(CMake/config_h_warning.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Werror -pthread")

set(TDHCPD_Config_File "/etc/tdhcpd.conf" CACHE STRING "Path to TDHCPD configuration file")
set(TDHCPD_Server_Port "67" CACHE STRING "Port number for server to listen on")
set(TDHCPD_Client_Port "68" CACHE STRING "Port number for server to respond on")

option(BUILD_TESTING "Enable unit tests" ON)
option(BUILD_LEASEVIEWER "Build the lease viewer program" ON)

configure_file(StaticConfig.h.in ${CMAKE_BINARY_DIR}/generated/StaticConfig.h @ONLY)

message("-- TDHCPD v${TDHCPD_VERSION}")
message("-- TDHCPD Config path: ${TDHCPD_Config_File}")
message("-- DHCP server port: ${TDHCPD_Server_Port}")
message("-- DHCP client port: ${TDHCPD_Client_Port}")

include_directories(${CMAKE_BINARY_DIR}/generated)
include_directories(${CMAKE_SOURCE_DIR})

add_library(${PROJECT_NAME}_IpConverter STATIC
    IpConverter.h
    IpConverter.cpp
)
set(IpConverterLib ${PROJECT_NAME}_IpConverter)

add_library(${PROJECT_NAME}_Serializer STATIC
    Serializer.h
    Serializer.cpp
)
set(SerializerLib ${PROJECT_NAME}_Serializer)

add_library(${PROJECT_NAME}_Network STATIC
    Network.h
    Network.cpp
)
set(NetworkLib ${PROJECT_NAME}_Network)

add_library(${PROJECT_NAME}_Configuration STATIC
    Configuration.h
    Configuration.cpp
)
set(ConfigurationLib ${PROJECT_NAME}_Configuration)

add_library(${PROJECT_NAME}_Logger STATIC
    Logger.h
    Logger.cpp
)
set(LoggerLib ${PROJECT_NAME}_Logger)

add_executable(${PROJECT_NAME}
    Structures.h
    Structures.cpp
    BootpSocket.h
    BootpSocket.cpp
    BootpHandler.h
    BootpHandler.cpp
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    ${IpConverterLib}
    ${SerializerLib}
    ${NetworkLib}
    ${ConfigurationLib}
    ${LoggerLib}
)

if (BUILD_TESTING)
    message("-- Will build unit tests")
    add_subdirectory(Tests)
else()
    message("-- Will NOT build unit tests")
endif()

if (BUILD_LEASEVIEWER)
    message("-- Will build the lease viewer")
    add_subdirectory(LeaseViewer)
else()
    message("-- Will NOT build the lease viewer")
endif()
