#===============================================================================
# MCP9700 driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   mcp9700, consumed as mcp9700::mcp9700 both through add_subdirectory() and
#   through find_package(mcp9700) after install.
#
#   cmake -S . -B build -DMCP9700_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/mcp9700_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${MCP9700_SOURCE_FILES}
        INCLUDE_DIRS ${MCP9700_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${MCP9700_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(MCP9700_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

string(CONCAT MCP9700_DESCRIPTION
    "C++20 driver for the Microchip MCP9700, MCP9700A, MCP9700B, MCP9701 and MCP9701A analog "
    "temperature sensors, read through your ADC, with clip, noise and range checks.")

project(mcp9700_driver
    VERSION ${MCP9700_VERSION}
    DESCRIPTION "${MCP9700_DESCRIPTION}"
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because mcp9700.hpp includes
# "../src/mcp9700.ipp" relative to itself:
#   <prefix>/include/mcp9700/inc/*.hpp, mcp9700_version.h
#   <prefix>/include/mcp9700/src/mcp9700.ipp
set(MCP9700_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/mcp9700/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${MCP9700_TARGET_NAME} INTERFACE)
target_include_directories(${MCP9700_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${MCP9700_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${MCP9700_INSTALL_INCLUDEDIR}>
)
target_compile_features(${MCP9700_TARGET_NAME} INTERFACE cxx_std_20)

# mcp9700::mcp9700 everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(mcp9700::mcp9700 ALIAS ${MCP9700_TARGET_NAME})
set_target_properties(${MCP9700_TARGET_NAME} PROPERTIES EXPORT_NAME mcp9700)

option(MCP9700_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links mcp9700::mcp9700" OFF)
if(MCP9700_ENABLE_WARNINGS)
    target_compile_options(${MCP9700_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(MCP9700_BUILD_TESTS "Build the host tests in tests/" OFF)
option(MCP9700_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(MCP9700_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${MCP9700_TARGET_NAME} EXPORT mcp9700Targets
    INCLUDES DESTINATION ${MCP9700_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${MCP9700_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${MCP9700_VERSION_HEADER}" DESTINATION ${MCP9700_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mcp9700/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT mcp9700Targets
    FILE mcp9700Targets.cmake
    NAMESPACE mcp9700::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp9700)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/mcp9700Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp9700Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp9700)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mcp9700ConfigVersion.cmake"
    VERSION ${MCP9700_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mcp9700Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp9700ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mcp9700)
