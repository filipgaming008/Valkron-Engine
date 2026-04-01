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

find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import jinja2"
    RESULT_VARIABLE GLAD_JINJA2_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT GLAD_JINJA2_RESULT EQUAL 0)
    message(FATAL_ERROR
        "GLAD requires Python package 'jinja2' to generate sources. "
        "Install dependencies with: python -m pip install -r Valkron/vendor/glad/requirements.txt"
    )
endif()

add_subdirectory(vendor/glad/cmake)
glad_add_library(glad_gl_core_46 STATIC REPRODUCIBLE API gl:core=4.6)
set_target_properties(glad_gl_core_46 PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui.cpp")
    message(FATAL_ERROR "Dear ImGui submodule not found! Please run: git submodule update --init --recursive")
endif()

set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/misc/cpp/imgui_stdlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(imgui
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/misc/cpp
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include
)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb/stb_image.h")
    message(FATAL_ERROR "stb submodule not found! Please run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/assimp/CMakeLists.txt")
    message(FATAL_ERROR "Assimp submodule not found! Please run: git submodule update --init --recursive")
endif()

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/assimp)

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
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
)

# Link GLFW and OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL glad_gl_core_46 imgui assimp::assimp)

# Platform detection
if(UNIX AND NOT APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VALKRON_PLATFORM_LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
elseif(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VALKRON_PLATFORM_WINDOWS)
    target_link_libraries(${PROJECT_NAME} PRIVATE Comdlg32)
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