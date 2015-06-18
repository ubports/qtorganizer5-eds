project(qorganizer-eds)

cmake_minimum_required(VERSION 2.8.9)

include(FindPkgConfig)
# Standard install paths
include(GNUInstallDirs)


find_package(Qt5Core REQUIRED)
add_definitions(-DQT_NO_KEYWORDS)
add_definitions(-std=c++11)

pkg_check_modules(GLIB REQUIRED glib-2.0>=2.32)
pkg_check_modules(GIO REQUIRED gio-2.0>=2.32)
pkg_check_modules(EDATASERVER REQUIRED libedataserver-1.2>=3.16)
pkg_check_modules(ECAL REQUIRED REQUIRED libecal-1.2>=3.16)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# Coverage tools
OPTION(ENABLE_COVERAGE "Build with coverage analysis support" OFF)
if(ENABLE_COVERAGE)
    message(STATUS "Using coverage flags")
    find_program(COVERAGE_COMMAND gcov)
    if(NOT COVERAGE_COMMAND)
        message(FATAL_ERROR "gcov command not found")
    endif()
    SET(CMAKE_C_FLAGS "-g -O0 -Wall -fprofile-arcs -ftest-coverage")
    SET(CMAKE_CXX_FLAGS "-g -O0 -Wall -fprofile-arcs -ftest-coverage")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov")
    include(${CMAKE_SOURCE_DIR}/cmake/lcov.cmake)
endif()

# Address Sanitizer
OPTION(ENABLE_ADDRSANITIZER "Build with address sanitizer support" OFF)
if(ENABLE_ADDRSANITIZER)
    message(STATUS "Using address sanitizer flags")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()


configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}"
                  -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

#Temporary dir used by tests
execute_process(COMMAND mktemp -d -t qorganizer_eds_test.XXX
                OUTPUT_VARIABLE  TMP_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)
#Create the directories
file(MAKE_DIRECTORY ${TMP_DIR}/.cache)
file(MAKE_DIRECTORY ${TMP_DIR}/.config)
file(MAKE_DIRECTORY ${TMP_DIR}/.local/share)

find_program(DBUS_RUNNER dbus-test-runner)
find_program(EVOLUTION_CALENDAR_FACTORY evolution-calendar-factory
             PATHS /usr/lib/evolution/)
find_program(EVOLUTION_SOURCE_REGISTRY evolution-source-registry
             PATHS /usr/lib/evolution/)
find_program(GVFSD gvfsd
             PATHS /usr/lib/gvfs/)
configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

OPTION(EVOLUTION_SOURCE_SERVICE_NAME "DBus name for source registry")
if(NOT EVOLUTION_SOURCE_SERVICE_NAME)
    set(EVOLUTION_SOURCE_SERVICE_NAME "org.gnome.evolution.dataserver.Sources3")
endif()

add_subdirectory(organizer)
if(DBUS_RUNNER)
    if(EVOLUTION_CALENDAR_FACTORY)
        enable_testing()
        add_subdirectory(tests)
    else(EVOLUTION_CALENDAR_FACTORY)
        message(WARNING "evolution-calendar-factory binary not found tests will be disabled")
    endif(EVOLUTION_CALENDAR_FACTORY)
else(DBUS_RUNNER)
    message(WARNING "dbus-test-runner binary not found tests will be disabled")
endif(DBUS_RUNNER)
