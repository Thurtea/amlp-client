cmake_minimum_required(VERSION 3.16)
project(amlp_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Windows icon
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

add_executable(amlp_client WIN32 main.cpp amlp_manage_connections.cpp ${APP_ICON_RESOURCE_WINDOWS})
target_link_libraries(amlp_client Qt6::Core Qt6::Widgets Qt6::Network)

# Installation rules and packaging
install(TARGETS amlp_client
    RUNTIME DESTINATION bin
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mudclient-icons
    DESTINATION share/amlp-client
)

# Also install any files windeployqt put into the package staging bin so CPack includes them
# Use the build tree package/bin where we stage files before packaging.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/package/bin/"
    DESTINATION bin
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING PATTERN "*"
    PATTERN "amlp_client.exe" EXCLUDE)

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "amlp-client")
set(CPACK_PACKAGE_VENDOR "Aether")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AMLP-Client - lightweight Qt6 MUD client")
# Generators: ZIP always available; NSIS if installed on Windows
set(CPACK_GENERATOR "ZIP;NSIS")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
endif()

include(CPack)
