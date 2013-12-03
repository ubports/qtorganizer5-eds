macro(declare_test TESTNAME)
    add_executable(${TESTNAME}
                    ${TESTNAME}.cpp
                    eds-base-test.cpp
                    eds-base-test.h
    )
    qt5_use_modules(${TESTNAME} Core Organizer Test)

    if(TEST_XML_OUTPUT)
        set(TEST_ARGS -p -xunitxml -p -o -p test_${testname}.xml)
    else()
        set(TEST_ARGS "")
    endif()

    target_link_libraries(${TESTNAME}
                          qtorganizer_eds-lib
                          ${GLIB_LIBRARIES}
                          ${GIO_LIBRARIES}
                          ${ECAL_LIBRARIES}
                          ${EDATASERVER_LIBRARIES}
    )

    add_test(${TESTNAME}
             ${DBUS_RUNNER} --keep-env
             --task ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME} --task-name ${TESTNAME} --wait-until-complete --wait-for=org.gnome.evolution.dataserver.Calendar4
             --task ${EVOLUTION_CALENDAR_FACTORY} --task-name "evolution" -r)
    update_test_properties(${TESTNAME})
endmacro(declare_test testname)

macro(update_test_properties TESTNAME)
    set(TEST_TMP_DIR "${TMP_DIR}/${TESTNAME}")
    list(APPEND ENV_VARS_LIST "QT_QPA_PLATFORM=minimal")
    list(APPEND ENV_VARS_LIST "HOME=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_CACHE_HOME=${TEST_TMP_DIR}/.cache")
    list(APPEND ENV_VARS_LIST "XDG_CONFIG_HOME=${TEST_TMP_DIR}/.config")
    list(APPEND ENV_VARS_LIST "XDG_DATA_HOME=${TEST_TMP_DIR}/.local/share")
    list(APPEND ENV_VARS_LIST "XDG_DESKTOP_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_DOCUMENTS_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_DOWNLOAD_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_MUSIC_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_PICTURES_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_PUBLICSHARE_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_TEMPLATES_DIR=${TEST_TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_VIDEOS_DIR=${TEST_TMP_DIR}")

    set_tests_properties(${TESTNAME} PROPERTIES
                         TIMEOUT ${CTEST_TESTING_TIMEOUT}
                         ENVIRONMENT "${ENV_VARS_LIST}")
endmacro(update_test_properties)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${qorganizer-eds-src_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${ECAL_INCLUDE_DIRS}
    ${EDATASERVER_INCLUDE_DIRS}
)

add_definitions(-DTEST_SUITE)
add_definitions(-std=c++11)
if(NOT CTEST_TESTING_TIMEOUT)
    set(CTEST_TESTING_TIMEOUT 60)
endif()

declare_test(fetchitem-test)
declare_test(itemid-test)
declare_test(parseecal-test)
declare_test(collections-test)
declare_test(event-test)
