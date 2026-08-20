#include "Logger.hpp"

#include <iostream>
#include <windows.h>


namespace {
    constexpr WORD COLOR_INFO = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    constexpr WORD COLOR_DEBUG = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    constexpr WORD COLOR_WARN = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    constexpr WORD COLOR_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;
    constexpr WORD COLOR_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    HANDLE GetConsole() { return GetStdHandle(STD_OUTPUT_HANDLE); }

    bool IsDebuggerAttached() { return IsDebuggerPresent(); }

    void Print(const char* level, const std::string& message, WORD color) {
        HANDLE console = GetConsole();
        SetConsoleTextAttribute(console, color);
        std::cout << '[' << level << "] ";
        SetConsoleTextAttribute(console, COLOR_WHITE);
        std::cout << message << '\n';
    }
}

namespace Logger {
    void Info(const std::string& message) { Print("INFO", message, COLOR_INFO); }
    void Debug(const std::string& message) {
        if (!IsDebuggerAttached()) return;
        Print("DEBUG", message, COLOR_DEBUG);
    }
    void Warn(const std::string& message) { Print("WARN", message, COLOR_WARN); }
    void Error(const std::string& message) { Print("ERROR", message, COLOR_ERROR); }
}