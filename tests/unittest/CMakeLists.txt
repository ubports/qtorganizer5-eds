macro(declare_test TESTNAME)
    add_executable(${TESTNAME}
                    ${TESTNAME}.cpp
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

    add_test(${TESTNAME} ${TESTNAME})
    set_tests_properties(${TESTNAME} PROPERTIES
                         TIMEOUT ${CTEST_TESTING_TIMEOUT}
                         ENVIRONMENT "QT_QPA_PLATFORM=minimal;${FOLKS_ENVIROMENT}")
endmacro(declare_test testname)

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
#declare_test(parseitem-test
