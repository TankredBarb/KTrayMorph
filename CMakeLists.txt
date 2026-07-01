cmake_minimum_required(VERSION 3.25)

project(KTrayMorph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick DBus Concurrent)
find_package(KF6 REQUIRED COMPONENTS Config I18n IconThemes Package)

qt_standard_project_setup(REQUIRES 6.6)

add_subdirectory(src)
add_subdirectory(plasmoid)

install(FILES icons/org.ktraymorph.plasmoid.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES icons/ktraymorph-update-symbolic.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/actions)

