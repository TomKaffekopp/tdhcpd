/*
 * TDHCPD - A Dynamic Host Configuration Protocol (DHCP) server
 * Copyright (C) 2024  Tom-Andre Barstad.
 * This software is licensed under the Software Attribution License.
 * See LICENSE for more information.
*/

#pragma once

#include <format>
#include <utility>
#include <functional>

namespace Log
{
enum class Level
{
    Debug,
    Info,
    Warning,
    Critical
};

namespace detail
{
void WriteLog(Level level, const std::string& text);
} // detail

using LogFunction = std::function<void(Level level, std::string_view)>;

std::string LogLevelPrefix(Level level);

Level ToLogLevel(std::string_view level);

void SetLogLevel(Level level);

void SetLogFunction(LogFunction&& function);

void UnsetLogFunction();

void Debug(std::string_view format, auto&&... args)
{
    const auto text = std::vformat(format, std::make_format_args(args...));

    detail::WriteLog(Level::Debug, text);
}

void Info(std::string_view format, auto&&... args)
{
    const auto text = std::vformat(format, std::make_format_args(args...));

    detail::WriteLog(Level::Info, text);
}

void Warning(std::string_view format, auto&&... args)
{
    const auto text = std::vformat(format, std::make_format_args(args...));

    detail::WriteLog(Level::Warning, text);
}

void Critical(std::string_view format, auto&&... args)
{
    const auto text = std::vformat(format, std::make_format_args(args...));

    detail::WriteLog(Level::Critical, text);
}

}