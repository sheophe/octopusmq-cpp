cmake_minimum_required(VERSION 3.8.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(octopusmq VERSION 1.2.0 LANGUAGES CXX)

option(OCTOMQ_ENABLE_TLS "Enable TLS support (OpenSSL is required)" OFF)
option(OCTOMQ_ENABLE_DDS "Enable DDS support (FastDDS is required)" OFF)
option(OCTOMQ_RELEASE_COMPILATION "Compile OctopusMQ with optimization and without debug data" OFF)
option(OCTOMQ_USE_STATIC_LIBS "Use static linkage to Boost libraries" OFF)

find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem)

if(OCTOMQ_ENABLE_TLS)
    message(STATUS "Building with TLS support")
    add_definitions(-DOCTOMQ_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
else()
    message(STATUS "Building without TLS support")
endif()

if(OCTOMQ_ENABLE_DDS)
    find_package(OpenDDS 3.14 REQUIRED)
    set(OPENDDS_CMAKE_VERBOSE ON)
    add_definitions(-DOCTOMQ_ENABLE_DDS)
endif()

if(OCTOMQ_USE_STATIC_LIBS)
    message(STATUS "Statically linking Boost")
    set(Boost_USE_STATIC_LIBS TRUE)
else()
    message(STATUS "Dynamically linking Boost")
    set(Boost_USE_STATIC_LIBS FALSE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

if(OCTOMQ_RELEASE_COMPILATION)
    message(STATUS "Building with optimization and without debug data")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()
    message(STATUS "Building without optimization and with debug data")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

set(SRC_DIR src)
set(LIB_DIR lib)

set(CORE_DIR ${SRC_DIR}/core)
set(NETWORK_DIR ${SRC_DIR}/network)
set(THREADS_DIR ${SRC_DIR}/threads)

set(LIB_JSON_DIR ${LIB_DIR}/json)
set(LIB_MQTT_DIR ${LIB_DIR}/mqtt_cpp)

set(SRC_LIST
    ${CORE_DIR}/log.cpp
    ${CORE_DIR}/settings.cpp
    ${CORE_DIR}/utility.cpp
    ${NETWORK_DIR}/message.cpp
    ${NETWORK_DIR}/network.cpp
    ${NETWORK_DIR}/adapter.cpp
    ${NETWORK_DIR}/adapter_factory.cpp
    ${NETWORK_DIR}/mqtt/adapter.cpp
    ${THREADS_DIR}/mqtt/broker.cpp
    ${THREADS_DIR}/control.cpp
    ${SRC_DIR}/octopus_mq.cpp
)

if(OCTOMQ_ENABLE_DDS)
    set(SRC_LIST
        ${SRC_LIST}
        ${NETWORK_DIR}/dds/adapter.cpp
        ${THREADS_DIR}/dds/peer.cpp
    )
endif()

include_directories(
    ${LIB_JSON_DIR}/single_include/nlohmann
    ${LIB_MQTT_DIR}/include
    ${SRC_DIR}
    ${CORE_DIR}
    ${NETWORK_DIR}
    ${THREADS_DIR}
)

link_directories(${Boost_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC pthread ${Boost_LIBRARIES})

if(OCTOMQ_ENABLE_DDS)
    set(OPENDDS_LIBS OpenDDS::Dcps OpenDDS::Tcp OpenDDS::Rtps OpenDDS::Rtps_Udp)
    set(OPENDDS_IDL_GENERATE_PATH "../${THREADS_DIR}/dds/message")
    OPENDDS_TARGET_SOURCES(${PROJECT_NAME} "${THREADS_DIR}/dds/message.idl"
                           TAO_IDL_OPTIONS -o ${OPENDDS_IDL_GENERATE_PATH}
                           OPENDDS_IDL_OPTIONS -o ${OPENDDS_IDL_GENERATE_PATH})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENDDS_LIBS})
endif()
