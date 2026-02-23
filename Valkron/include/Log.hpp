#pragma once

#include <iostream>

// Simple colored output
enum class LogColor {
    RED, GREEN, YELLOW, BLUE, RESET
};

#ifdef _WIN32
    #include <windows.h>
    inline void setColor(LogColor color) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        switch(color) {
            case LogColor::RED:    SetConsoleTextAttribute(hConsole, 12); break;
            case LogColor::GREEN:  SetConsoleTextAttribute(hConsole, 10); break;
            case LogColor::YELLOW: SetConsoleTextAttribute(hConsole, 14); break;
            case LogColor::BLUE:   SetConsoleTextAttribute(hConsole, 9); break;
            default:                SetConsoleTextAttribute(hConsole, 7); break;
        }
    }
#else
    inline void setColor(LogColor color) {
        switch(color) {
            case LogColor::RED:    std::cout << "\033[31m"; break;
            case LogColor::GREEN:  std::cout << "\033[32m"; break;
            case LogColor::YELLOW: std::cout << "\033[33m"; break;
            case LogColor::BLUE:   std::cout << "\033[34m"; break;
            default:                std::cout << "\033[0m"; break;
        }
    }
#endif

// Simple logging functions
inline void logInfo(const std::string& msg) {
    setColor(LogColor::GREEN);
    std::cout << "[INFO] " << msg << std::endl;
    setColor(LogColor::RESET);
}

inline void logWarn(const std::string& msg) {
    setColor(LogColor::YELLOW);
    std::cout << "[WARN] " << msg << std::endl;
    setColor(LogColor::RESET);
}

inline void logError(const std::string& msg) {
    setColor(LogColor::RED);
    std::cerr << "[ERROR] " << msg << std::endl;
    setColor(LogColor::RESET);
}

inline void logDebug(const std::string& msg) {
    setColor(LogColor::BLUE);
    std::cout << "[DEBUG] " << msg << std::endl;
    setColor(LogColor::RESET);
}