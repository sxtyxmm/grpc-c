cmake_minimum_required(VERSION 3.10)
project(grpc-c VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

# Source files
set(GRPC_SOURCES
    src/grpc_core.c
    src/grpc_channel.c
    src/grpc_server.c
    src/grpc_credentials.c
    src/http2_transport.c
)

# Static library
add_library(grpc-c-static STATIC ${GRPC_SOURCES})
set_target_properties(grpc-c-static PROPERTIES OUTPUT_NAME grpc-c)

# Shared library
add_library(grpc-c-shared SHARED ${GRPC_SOURCES})
set_target_properties(grpc-c-shared PROPERTIES OUTPUT_NAME grpc-c)

# Link pthread
target_link_libraries(grpc-c-static pthread)
target_link_libraries(grpc-c-shared pthread)

# Platform-specific libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(grpc-c-static rt)
    target_link_libraries(grpc-c-shared rt)
endif()

# Tests
enable_testing()
add_executable(basic_test test/basic_test.c)
target_link_libraries(basic_test grpc-c-static)
add_test(NAME BasicTest COMMAND basic_test)

# Examples
add_executable(echo_server examples/echo_server.c)
target_link_libraries(echo_server grpc-c-static)

add_executable(echo_client examples/echo_client.c)
target_link_libraries(echo_client grpc-c-static)

# Installation
install(TARGETS grpc-c-static grpc-c-shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/grpc DESTINATION include)

# pkg-config file
configure_file(grpc-c.pc.in grpc-c.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/grpc-c.pc DESTINATION lib/pkgconfig)

# Print summary
message(STATUS "")
message(STATUS "grpc-c configuration summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C flags: ${CMAKE_C_FLAGS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
