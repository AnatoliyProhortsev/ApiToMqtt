cmake_minimum_required(VERSION 3.10)
project(ApiToMqtt)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PahoMqttCpp REQUIRED)

include_directories(${LIBXML2_INCLUDE_DIR})

add_executable(ApiToMqtt ApiToMqtt.cpp)
target_link_libraries(ApiToMqtt
    xml2
    curl
    paho-mqttpp3
    paho-mqtt3as)


