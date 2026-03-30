cmake_minimum_required(VERSION 4.2 FATAL_ERROR)
project(Valkron VERSION 0.0.1)

# Dependencies
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/CMakeLists.txt")
    message(FATAL_ERROR "GLFW submodule not found! Please run: git submodule update --init --recursive")
endif()
add_subdirectory(vendor/glfw)
add_subdirectory(vendor/glm)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/cmake/CMakeLists.txt")
    message(FATAL_ERROR "GLAD submodule not found! Please run: git submodule update --init --recursive")
endif()
add_subdirectory(vendor/glad/cmake)
glad_add_library(glad_gl_core_46 STATIC REPRODUCIBLE API gl:core=4.6)
set_target_properties(glad_gl_core_46 PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb/stb_image.h")
    message(FATAL_ERROR "stb submodule not found! Please run: git submodule update --init --recursive")
endif()

# Add source & header files
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Create the Valkron shared library
add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES} ${PROJECT_HEADERS})

# Include directories (PUBLIC so consumers like App get them too)
target_include_directories(${PROJECT_NAME}
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
)

# Link GLFW and OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL glad_gl_core_46)

# Platform detection
if(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VALKRON_PLATFORM_LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
elseif(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VALKRON_PLATFORM_WINDOWS)
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE VALKRON_BUILD_DLL)

# Tag engine sources so logging knows the source
target_compile_definitions(${PROJECT_NAME} PRIVATE VALKRON_BUILD_ENGINE)

# Set output directories
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/src"
)