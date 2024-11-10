cmake_minimum_required(VERSION 3.10)
project(SecurityCameraApp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(MOSQUITTO_DIR "C:/Program Files/mosquitto")
set(CURL_DIR "C:/lib/curl")

include_directories(
    ${MOSQUITTO_DIR}/devel
    ${CURL_DIR}/include
)

link_directories(
    ${MOSQUITTO_DIR}/devel
    ${CURL_DIR}/lib
)

add_executable(SecurityCameraApp main.cpp)

target_link_libraries(SecurityCameraApp
    mosquitto
    libcurl
    ws2_32.lib
)
