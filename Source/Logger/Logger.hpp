#pragma once

#include <string>
#include <format>

namespace Logger {
    void Info(const std::string& message);
    void Debug(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);

    template<typename... Args>
    void Info(std::format_string<Args...> format, Args&&... args) { Info(std::format(format, std::forward<Args>(args)...)); }

    template<typename... Args>
    void Debug(std::format_string<Args...> format, Args&&... args) { Debug(std::format(format, std::forward<Args>(args)...)); }

    template<typename... Args>
    void Warn(std::format_string<Args...> format, Args&&... args) { Warn(std::format(format, std::forward<Args>(args)...)); }

    template<typename... Args>
    void Error(std::format_string<Args...> format, Args&&... args) { Error(std::format(format, std::forward<Args>(args)...)); }
}