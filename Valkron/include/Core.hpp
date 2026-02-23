#pragma once

#ifdef VALKRON_PLATFORM_WINDOWS
    #ifdef VALKRON_BUILD_DLL
        #define VALKRON_API __declspec(dllexport)
    #else
        #define VALKRON_API __declspec(dllimport)
    #endif
#elif defined(VALKRON_PLATFORM_LINUX)
    #ifdef VALKRON_BUILD_DLL
        #define VALKRON_API __attribute__((visibility("default")))
    #else
        #define VALKRON_API
    #endif
#else
    #define VALKRON_API
#endif