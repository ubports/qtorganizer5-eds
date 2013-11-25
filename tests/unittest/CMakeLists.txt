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
             ${DBUS_RUNNER} --keep-env -m 90
             --task ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME} --task-name ${TESTNAME} -c --wait-for=org.gnome.evolution.dataserver.Calendar4
             --task ${EVOLUTION_CALENDAR_FACTORY} --task-name evolution -r)
    update_test_properties(${TESTNAME})
endmacro(declare_test testname)

macro(update_test_properties TESTNAME)

    execute_process(COMMAND mktemp -d -t qorganizer_eds_test.XXX
                    OUTPUT_VARIABLE  TMP_DIR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    #Create the directories
    file(MAKE_DIRECTORY ${TMP_DIR}/.cache)
    file(MAKE_DIRECTORY ${TMP_DIR}/.config)
    file(MAKE_DIRECTORY ${TMP_DIR}/.local/share)

    list(APPEND ENV_VARS_LIST "QT_QPA_PLATFORM=minimal")
    list(APPEND ENV_VARS_LIST "HOME=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_CACHE_HOME=${TMP_DIR}/.cache")
    list(APPEND ENV_VARS_LIST "XDG_CONFIG_HOME=${TMP_DIR}/.config")
    list(APPEND ENV_VARS_LIST "XDG_DATA_HOME=${TMP_DIR}/.local/share")
    list(APPEND ENV_VARS_LIST "XDG_DESKTOP_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_DOCUMENTS_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_DOWNLOAD_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_MUSIC_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_PICTURES_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_PUBLICSHARE_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_TEMPLATES_DIR=${TMP_DIR}")
    list(APPEND ENV_VARS_LIST "XDG_VIDEOS_DIR=${TMP_DIR}")

    set_tests_properties(${TESTNAME} PROPERTIES
                         TIMEOUT ${CTEST_TESTING_TIMEOUT}
                         ENVIRONMENT "${ENV_VARS_LIST}")

endmacro(update_test_properties)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${qorganizer-eds-src_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${ECAL_INCLUDE_DIRS}
    ${EDATASERVER_INCLUDE_DIRS}
)

add_definitions(-DTEST_SUITE)
if(NOT CTEST_TESTING_TIMEOUT)
    set(CTEST_TESTING_TIMEOUT 60)
endif()

declare_test(itemid-test)
declare_test(parseecal-test)
declare_test(collections-test)
declare_test(event-test)
