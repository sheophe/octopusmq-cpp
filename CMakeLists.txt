cmake_minimum_required(VERSION 3.8.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR src)
set(LIB_DIR lib)

set(CORE_DIR ${SRC_DIR}/core)
set(NETWORK_DIR ${SRC_DIR}/network)
set(THREADS_DIR ${SRC_DIR}/threads)

set(LIB_JSON_DIR ${LIB_DIR}/json)
set(LIB_MQTT_DIR ${LIB_DIR}/mqtt_cpp)

project(octopusmq VERSION 1.0.0 LANGUAGES CXX)

option(OCTOMQ_ENABLE_TLS "Enable TLS support (OpenSSL is required)" OFF)
option(OCTOMQ_RELEASE_COMPILATION "Compile OctopusMQ with optimization and without debug data" OFF)
option(OCTOMQ_USE_STATIC_LIBS "Use static linkage to Boost and OpenDDS libraries" OFF)

find_package(OpenDDS 3.14 REQUIRED)
find_package(Boost 1.66.0 REQUIRED COMPONENTS system REQUIRED)
find_path(BOOST_ASIO_INCLUDE_DIRS boost/asio.hpp)

set(OPENDDS_CMAKE_VERBOSE ON)

if (OCTOMQ_RELEASE_COMPILATION)
    message(STATUS "Builiding with optimization and without debug data")
else()
    message(STATUS "Building without optimization and with debug data")
endif()

if (OCTOMQ_ENABLE_TLS)
    message(STATUS "Building with TLS support")
    add_definitions(-DOCTOMQ_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
else()
    message(STATUS "Building without TLS support")
endif()

if (OCTOMQ_USE_STATIC_LIBS)
    message (STATUS "Staticly linking with Boost")
    set (Boost_USE_STATIC_LIBS TRUE)
    message (STATUS "Staticly linking with OpenDDS")
    set (OPENDDS_STATIC ON)
else()
    message (STATUS "Dynamically linking with Boost")
    set (Boost_USE_STATIC_LIBS FALSE)
    message (STATUS "Dynamically linking with OpenDDS")
    set (OPENDDS_STATIC OFF)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

if (OCTOMQ_RELEASE_COMPILATION)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(SRC_LIST
    ${CORE_DIR}/log.cpp
    ${CORE_DIR}/settings.cpp
    ${NETWORK_DIR}/message.cpp
    ${NETWORK_DIR}/network.cpp
    ${NETWORK_DIR}/adapter.cpp
    ${NETWORK_DIR}/adapter_factory.cpp
    ${NETWORK_DIR}/adapter_headers.hpp
    ${NETWORK_DIR}/dds/adapter.cpp
    ${NETWORK_DIR}/mqtt/adapter.cpp
    ${THREADS_DIR}/dds/peer.cpp
    ${THREADS_DIR}/mqtt/broker.cpp
    ${THREADS_DIR}/control.cpp
    ${SRC_DIR}/octopus_mq.cpp
)

include_directories(
    ${LIB_JSON_DIR}/single_include/nlohmann
    ${LIB_MQTT_DIR}/include
    ${SRC_DIR}
    ${CORE_DIR}
    ${NETWORK_DIR}
    ${THREADS_DIR}
)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${BOOST_ASIO_INCLUDE_DIRS})

set(OPENDDS_LIBS OpenDDS::Dcps OpenDDS::Tcp OpenDDS::Rtps OpenDDS::Rtps_Udp)
target_link_libraries(${PROJECT_NAME} PUBLIC pthread ${OPENDDS_LIBS})

set(OPENDDS_IDL_GENERATE_PATH "../${THREADS_DIR}/dds/message")
OPENDDS_TARGET_SOURCES(${PROJECT_NAME} "${THREADS_DIR}/dds/message.idl"
                       TAO_IDL_OPTIONS -o ${OPENDDS_IDL_GENERATE_PATH}
                       OPENDDS_IDL_OPTIONS -o ${OPENDDS_IDL_GENERATE_PATH})

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()
