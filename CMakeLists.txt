cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR src)
set(CORE_DIR ${SRC_DIR}/core)
set(NETWORK_DIR ${SRC_DIR}/network)
set(THREADS_DIR ${SRC_DIR}/threads)

project(octopusmq VERSION 1.0.0 LANGUAGES CXX)

# find_package(OpenDDS REQUIRED)

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
    ${THREADS_DIR}/mqtt/broker.cpp
    ${SRC_DIR}/octopus_mq.cpp
)

include_directories(
    ${SRC_DIR}
    ${CORE_DIR}
    ${NETWORK_DIR}
    ${THREADS_DIR}
)

add_executable(${PROJECT_NAME} ${SRC_LIST})

if (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} stdc++fs)
endif()

# OPENDDS_TARGET_SOURCES(${PROJECT_NAME} "${THREADS_DIR}/dds/message.idl" OPENDDS_IDL_OPTIONS "-Lc++11")
target_link_libraries(${PROJECT_NAME} pthread)#${OPENDDS_LIBS} pthread)
