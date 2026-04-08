cmake_minimum_required(VERSION 3.16)
project(capnp_demo VERSION 1.0.0 LANGUAGES CXX)

# Cap'n Proto 2.0 requires C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Cap'n Proto
find_package(CapnProto CONFIG REQUIRED)

# Set proto file paths
set(CAPNP_SCHEMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(CAPNP_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Create output directory
file(MAKE_DIRECTORY ${CAPNP_OUTPUT_DIR})

# Compile Cap'n Proto schema
set(CAPNP_SCHEMA ${CAPNP_SCHEMA_DIR}/messages.capnp)
set(CAPNP_GENERATED_SOURCES
    ${CAPNP_OUTPUT_DIR}/messages.capnp.c++
    ${CAPNP_OUTPUT_DIR}/messages.capnp.h
)

add_custom_command(
    OUTPUT ${CAPNP_GENERATED_SOURCES}
    COMMAND ${CAPNP_EXECUTABLE} compile
            -o ${CAPNPC_CXX_EXECUTABLE}:${CAPNP_OUTPUT_DIR}
            --src-prefix=${CAPNP_SCHEMA_DIR}
            ${CAPNP_SCHEMA}
    DEPENDS ${CAPNP_SCHEMA}
    COMMENT "Compiling Cap'n Proto schema"
)

# Create library for generated code
add_library(capnp_messages STATIC ${CAPNP_GENERATED_SOURCES})
target_include_directories(capnp_messages PUBLIC ${CAPNP_OUTPUT_DIR})
target_link_libraries(capnp_messages PUBLIC CapnProto::capnp-rpc)

# Core executable
add_executable(core src/core.cpp)
target_link_libraries(core PRIVATE capnp_messages CapnProto::capnp-rpc)
target_include_directories(core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Ext executable
add_executable(ext src/ext.cpp)
target_link_libraries(ext PRIVATE capnp_messages CapnProto::capnp-rpc)
target_include_directories(ext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Install targets
install(TARGETS core ext RUNTIME DESTINATION bin)

# Print configuration information
message(STATUS "Cap'n Proto version: ${CapnProto_VERSION}")
message(STATUS "Cap'n Proto executable: ${CAPNP_EXECUTABLE}")
message(STATUS "Generated files directory: ${CAPNP_OUTPUT_DIR}")
