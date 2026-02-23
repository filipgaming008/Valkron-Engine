#pragma once

#ifdef VALKRON_PLATFORM_WINDOWS
    #ifdef VALKRON_BUILD_DLL
        #define VALKRON_API __declspec(dllexport)
    #else
        #define VALKRON_API __declspec(dllimport)
    #endif
#else
    #define VALKRON_API
#endif