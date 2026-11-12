#===============================================================================
# MCP9700 driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(MCP9700_TARGET_NAME "mcp9700")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(MCP9700_VERSION_MAJOR 0)
set(MCP9700_VERSION_MINOR 1)
set(MCP9700_VERSION_PATCH 0)
set(MCP9700_VERSION "${MCP9700_VERSION_MAJOR}.${MCP9700_VERSION_MINOR}.${MCP9700_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(MCP9700_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/mcp9700_version.h.in")
set(MCP9700_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/mcp9700_generated")
set(MCP9700_VERSION_HEADER "${MCP9700_VERSION_HEADER_DIR}/mcp9700_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${MCP9700_VERSION_HEADER_DIR}")
configure_file("${MCP9700_VERSION_TEMPLATE}" "${MCP9700_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(MCP9700_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${MCP9700_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/mcp9700.ipp, included by inc/mcp9700.hpp.
set(MCP9700_SOURCE_FILES)

set(MCP9700_IDF_REQUIRES)
