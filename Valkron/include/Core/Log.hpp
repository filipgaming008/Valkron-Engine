#pragma once

#include <cassert>

// Auto-detect source: Engine or App
#ifdef VALKRON_BUILD_ENGINE
    #define VALKRON_LOG_SOURCE "Valkron"
#else
    #define VALKRON_LOG_SOURCE "App"
#endif

// Logging macros — compiled out in release (NDEBUG)
#ifdef NDEBUG
    #define LOG_INFO(msg)  ((void)0)
    #define LOG_WARN(msg)  ((void)0)
    #define LOG_ERROR(msg) ((void)0)
    #define LOG_DEBUG(msg) ((void)0)
    #define VALKRON_ASSERT(expr, msg)      ((void)0)
    #define VALKRON_CORE_ASSERT(expr, msg) ((void)0)
#else
    #include <iostream>
    #include <string>

    #define LOG_INFO(msg) \
        std::cout << "\033[32m[INFO] [" << VALKRON_LOG_SOURCE << "] " << (msg) << "\033[0m" << std::endl

    #define LOG_WARN(msg) \
        std::cout << "\033[33m[WARN] [" << VALKRON_LOG_SOURCE << "] " << (msg) << "\033[0m" << std::endl

    #define LOG_ERROR(msg) \
        std::cerr << "\033[31m[ERROR] [" << VALKRON_LOG_SOURCE << "] " << (msg) << "\033[0m" << std::endl

    #define LOG_DEBUG(msg) \
        std::cout << "\033[34m[DEBUG] [" << VALKRON_LOG_SOURCE << "] " << (msg) << "\033[0m" << std::endl

    #define VALKRON_ASSERT(expr, msg) \
        do { \
            const bool _valkron_assert_ok = static_cast<bool>(expr); \
            if (!_valkron_assert_ok) { \
                LOG_ERROR(std::string("Assertion failed: ") + #expr + " | " + (msg)); \
                assert(_valkron_assert_ok && (msg)); \
            } \
        } while (0)

    #define VALKRON_CORE_ASSERT(expr, msg) \
        do { \
            const bool _valkron_core_assert_ok = static_cast<bool>(expr); \
            if (!_valkron_core_assert_ok) { \
                LOG_ERROR(std::string("Core assertion failed: ") + #expr + " | " + (msg)); \
                assert(_valkron_core_assert_ok && (msg)); \
            } \
        } while (0)
#endif