/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "BootpSocket.h"
#include "BootpHandler.h"
#include "Configuration.h"
#include "StaticConfig.h"
#include "Logger.h"

#include <unistd.h>
#include <syslog.h>

#include <csignal>
#include <ctime>

#include <forward_list>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <fstream>

namespace
{
std::atomic_bool running{ true };
std::condition_variable cv_running;
std::mutex cv_m;
std::ofstream logfile;

void sigtermFn(int)
{
    Log::Info("Exiting TDHCPD...");
    BootpHandler::stop();
    running = false;
    cv_running.notify_one();
}

void daemonize()
{
    auto pid = fork();
    if (pid < 0)
        std::exit(-1);
    if (pid > 0)
        std::exit(0);
    if (setsid() < 0)
        std::exit(-1);

    pid = fork();
    if (pid < 0)
        std::exit(-1);
    if (pid > 0)
        std::exit(0);
}

std::unordered_map<std::string, Network> createNetworks(std::span<std::string> interfaces)
{
    std::unordered_map<std::string, Network> network;

    for (const auto& interface : interfaces)
    {
        auto config = Configuration::GetNetworkConfiguration(interface);
        auto leases = Configuration::GetPersistentLeasesByInterface(interface);
        network[interface].configure(std::move(config), leases);
    }

    return network;
}

void LogToSyslog(Log::Level level, std::string_view text)
{
    static auto levelToSyslogLevel = [](Log::Level level) -> int
    {
        switch (level)
        {
            case Log::Level::Debug:
                return LOG_DEBUG;
            case Log::Level::Info:
                return LOG_INFO;
            case Log::Level::Warning:
                return LOG_WARNING;
            case Log::Level::Critical:
                return LOG_CRIT;
        }
        // GCC wants this even though all cases are covered :(
        return LOG_INFO;
    };

    syslog(levelToSyslogLevel(level), "%s", text.data());
}

void LogToFile(Log::Level level, std::string_view text)
{
    auto now = std::time(nullptr);
    std::string timestamp = std::ctime(&now);
    timestamp.pop_back(); // \n from ctime()

    logfile << timestamp << " " << Log::LogLevelPrefix(level) << text << "\n";
    logfile.flush();
}

void setupSyslog()
{
    Log::SetLogFunction(&LogToSyslog);
    openlog("TDHCPD", 0, LOG_DAEMON);
}

void setupFilelog()
{
    logfile.open(Configuration::GetLogFileName(), std::ios::out | std::ios::app);
    if (!logfile.is_open())
    {
        Log::UnsetLogFunction();
        Log::Critical("Couldn't open {} for logging, using console", Configuration::GetLogFileName());
    }

    Log::SetLogFunction(&LogToFile);
}

void setupLogging()
{
    Log::SetLogLevel(Configuration::GetLogLevel());

    if (!Configuration::GetLogFileName().empty())
    {
        setupFilelog();
    }
    else if (!Configuration::GetPidFileName().empty())
    {
        setupSyslog();
    }
}

void closeLogging()
{
    if (!Configuration::GetLogFileName().empty())
    {
        logfile.close();
    }
    else if (!Configuration::GetPidFileName().empty())
    {
        closelog();
    }
}

} // anonymous ns

int main()
{
    if (!Configuration::LoadFromFile(StaticConfig::ConfigFile))
        return -1;

    if (!Configuration::GetPidFileName().empty())
    {
        daemonize();

        struct sigaction sighandler{};
        sighandler.sa_handler = &sigtermFn;
        sigaction(SIGCHLD, &sighandler, nullptr);
    }

    setupLogging();

    if (!Configuration::GetPidFileName().empty())
    {
        /* Write pid file */
    }

    /* SIGTERM or SIGCHLD */
    {
        struct sigaction sighandler{};
        sighandler.sa_handler = &sigtermFn;
        sigaction(SIGTERM, &sighandler, nullptr);
    }

    /* SIGINT */
    {
        struct sigaction sighandler{};
        sighandler.sa_handler = &sigtermFn;
        sigaction(SIGINT, &sighandler, nullptr);
    }

    auto pid = getpid();

    Log::Info("Starting TDHCPD[{}], server serverPort {}, clientPort {}", pid, StaticConfig::ServerPort, StaticConfig::ClientPort);

    auto interfaces = Configuration::GetConfiguredInterfaces();

    BootpHandler::start(createNetworks(interfaces));

    std::forward_list<BootpSocket> sockets;

    for (const auto& interface : interfaces)
        sockets.emplace_front(StaticConfig::ServerPort, StaticConfig::ClientPort, interface);

    /*
     * Put main thread to sleep since it doesn't have anything more to do.
     * SIGTERM will unblock the condition variable and terminate the program.
     *
     * TODO propagate any errors during start (ie. bind error, etc) so that we can exit.
    */
    {
        std::unique_lock lk(cv_m);
        cv_running.wait(lk, [] { return !running; });
    }

    sockets.clear();

    closeLogging();

    Log::Info("Thank you for playing.");

    return 0;
}
