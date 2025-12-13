cmake_minimum_required(VERSION 3.15)
project(blackbox-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Download Boost headers (full release with all headers)
FetchContent_Declare(
    boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz
)
FetchContent_GetProperties(boost)
if(NOT boost_POPULATED)
    FetchContent_Populate(boost)
    set(BOOST_BUILD_DIR ${CMAKE_BINARY_DIR}/boost_build)
    file(MAKE_DIRECTORY ${BOOST_BUILD_DIR})
    # Build only system library (required by asio)
    execute_process(
        COMMAND ./bootstrap.sh --with-libraries=system --prefix=${CMAKE_BINARY_DIR}/boost_install
        WORKING_DIRECTORY ${boost_SOURCE_DIR}
        RESULT_VARIABLE bootstrap_result
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT bootstrap_result EQUAL 0)
        message(FATAL_ERROR "Boost bootstrap failed")
    endif()
    execute_process(
        COMMAND ./b2 --with-system --prefix=${CMAKE_BINARY_DIR}/boost_install variant=release link=static threading=multi -j$(nproc)
        WORKING_DIRECTORY ${boost_SOURCE_DIR}
        RESULT_VARIABLE b2_result
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT b2_result EQUAL 0)
        message(FATAL_ERROR "Boost b2 build failed")
    endif()
    execute_process(
        COMMAND ./b2 install
        WORKING_DIRECTORY ${boost_SOURCE_DIR}
        RESULT_VARIABLE install_result
    )
endif()

# Download Abseil (header-only)
FetchContent_Declare(
    abseil
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20240116.2
)
FetchContent_MakeAvailable(abseil)

# Check for NVML (optional)
find_path(NVML_INCLUDE_DIR nvml.h
    PATHS
    /usr/include
    /usr/local/include
    /opt/nvidia/include
)

if(NVML_INCLUDE_DIR)
    message(STATUS "Found NVML: ${NVML_INCLUDE_DIR}")
    add_definitions(-DNVML_AVAILABLE)
    include_directories(${NVML_INCLUDE_DIR})
    
    find_library(NVML_LIB nvidia-ml
        PATHS
        /usr/lib
        /usr/lib64
        /usr/local/lib
        /opt/nvidia/lib
    )
    
    if(NVML_LIB)
        message(STATUS "Found NVML library: ${NVML_LIB}")
    endif()
endif()

add_executable(blackbox-server src/main.cpp)

target_include_directories(blackbox-server PRIVATE
    ${boost_SOURCE_DIR}
    ${abseil_SOURCE_DIR}
)

target_link_directories(blackbox-server PRIVATE
    ${CMAKE_BINARY_DIR}/boost_install/lib
)

target_link_libraries(blackbox-server
    boost_system
    absl::strings
    absl::str_format_internal
)

if(NVML_LIB)
    target_link_libraries(blackbox-server ${NVML_LIB})
endif()
