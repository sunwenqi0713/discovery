############################################################
# discovery - Network Device Discovery Library
############################################################

cmake_minimum_required(VERSION 3.16)

project(discovery
    VERSION 1.0.0
    DESCRIPTION "Network Device Discovery Library"
    LANGUAGES CXX
)

# C++17 is required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(discovery_BUILD_SHARED "Build shared library" OFF)
option(discovery_BUILD_EXAMPLES "Build examples" ON)

# Source files
set(discovery_HEADERS
    include/discovery/discovery_protocol_version.h
    include/discovery/discovery_protocol.h
    include/discovery/discovery_ip_port.h
    include/discovery/discovery_peer_parameters.h
    include/discovery/discovery_peer.h
    include/discovery/discovery_discovered_peer.h
)

set(discovery_SOURCES
    src/discovery_protocol.cpp
    src/discovery_ip_port.cpp
    src/discovery_peer.cpp
)

# Create library
if(discovery_BUILD_SHARED)
    add_library(discovery SHARED ${discovery_SOURCES} ${discovery_HEADERS})
    target_compile_definitions(discovery PRIVATE discovery_EXPORTS)
else()
    add_library(discovery STATIC ${discovery_SOURCES} ${discovery_HEADERS})
    target_compile_definitions(discovery PUBLIC discovery_STATIC)
endif()

# Include directories
target_include_directories(discovery
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(discovery PRIVATE ws2_32)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(discovery PRIVATE /W4)
else()
    target_compile_options(discovery PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Alias for modern CMake
add_library(discovery::discovery ALIAS discovery)

# Examples
if(discovery_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS discovery
    EXPORT discoveryTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/discovery
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT discoveryTargets
    FILE discoveryTargets.cmake
    NAMESPACE discovery::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/discovery
)

# Generate Config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/discoveryConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/discoveryConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/discoveryConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/discovery
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/discoveryConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/discoveryConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/discovery
)

# Summary
message(STATUS "")
message(STATUS "discovery ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${discovery_BUILD_SHARED}")
message(STATUS "  Examples: ${discovery_BUILD_EXAMPLES}")
message(STATUS "")
