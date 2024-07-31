/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#include "Logger.h"

#include <mutex>
#include <iostream>

namespace
{
Log::Level LogLevel = Log::Level::Info;
Log::LogFunction Logger;

void StdoutLogger(Log::Level level, std::string_view text)
{
    static std::mutex mutex;
    std::lock_guard lockGuard(mutex);

    auto now = std::time(nullptr);
    std::string timestamp = std::ctime(&now);
    timestamp.pop_back(); // \n from ctime()

    std::cout << timestamp << " " << LogLevelPrefix(level) << text << std::endl;
}
}

void Log::detail::WriteLog(Log::Level level, const std::string& text)
{
    const auto currentLogLevelNum = static_cast<int>(LogLevel);
    const auto logLevelNum = static_cast<int>(level);
    if (logLevelNum < currentLogLevelNum)
        return;

    if (Logger)
        Logger(level, text);
    else
        StdoutLogger(level, text);
}

std::string Log::LogLevelPrefix(Log::Level level)
{
    using namespace Log;
    switch (level)
    {
        case Level::Debug:
            return "[D] ";
        case Level::Info:
            return "[I] ";
        case Level::Warning:
            return "[W] ";
        case Level::Critical:
            return "[C] ";
    }
    // GCC wants this even though all cases are covered :(
    return "[I] ";
}

void Log::SetLogFunction(Log::LogFunction&& function)
{
    Logger = std::move(function);
}

void Log::UnsetLogFunction()
{
    Logger = &StdoutLogger;
}

Log::Level Log::ToLogLevel(std::string_view level)
{
    if (level == "debug")
        return Level::Debug;

    if (level == "warning")
        return Level::Debug;

    if (level == "critical")
        return Level::Debug;

    return Level::Info;
}

void Log::SetLogLevel(Log::Level level)
{
    LogLevel = level;
}
