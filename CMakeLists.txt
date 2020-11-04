cmake_minimum_required(VERSION 3.8)
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

# find_package(OpenDDS REQUIRED)

find_package(Boost 1.66.0 REQUIRED COMPONENTS system REQUIRED)
find_path(BOOST_ASIO_INCLUDE_DIRS boost/asio.hpp)

if (OCTOMQ_ENABLE_TLS)
    message(STATUS "Configuring to support TLS")
    find_package(OpenSSL REQUIRED)
else()
    message(STATUS "Configuring without TLS support")
endif()

# set(OPENDDS_LIBS OpenDDS::Dcps)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic -g")

set(SRC_LIST
    ${NETWORK_DIR}/network.cpp
    ${NETWORK_DIR}/adapter.cpp
    ${NETWORK_DIR}/adapter_factory.cpp
    ${NETWORK_DIR}/adapter_headers.hpp
    ${NETWORK_DIR}/dds/adapter.cpp
    ${NETWORK_DIR}/mqtt/adapter.cpp
    ${CORE_DIR}/log.cpp
    ${CORE_DIR}/settings.cpp
    ${CORE_DIR}/topic.cpp
    ${CORE_DIR}/event_queue.cpp
    ${CORE_DIR}/message_queue.cpp
    ${THREADS_DIR}/control.cpp
    ${THREADS_DIR}/mqtt/tcp_broker.cpp
    ${THREADS_DIR}/mqtt/ws_broker.cpp
    ${SRC_DIR}/octopus_mq.cpp
)

if (OCTOMQ_ENABLE_TLS)
    set(SRC_LIST
        ${SRC_LIST}
        ${SRC_DIR}/mqtt/tls_broker.cpp
        ${SRC_DIR}/mqtt/tls_ws_broker.cpp
    )
endif()

set(SRC_LIST
    ${SRC_LIST}
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

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()

# OPENDDS_TARGET_SOURCES(${PROJECT_NAME} "${THREADS_DIR}/dds/message.idl" OPENDDS_IDL_OPTIONS "-Lc++11")
target_link_libraries(${PROJECT_NAME} pthread)
