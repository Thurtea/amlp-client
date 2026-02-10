cmake_minimum_required(VERSION 3.16)
project(amlp_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Windows icon
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

add_executable(amlp_client WIN32 main.cpp ${APP_ICON_RESOURCE_WINDOWS})
target_link_libraries(amlp_client Qt6::Core Qt6::Widgets Qt6::Network)
