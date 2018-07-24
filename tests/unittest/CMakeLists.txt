macro(declare_test TESTNAME)
    add_executable(${TESTNAME}
                    ${TESTNAME}.cpp
                    eds-base-test.cpp
                    eds-base-test.h
                    gscopedpointer.h
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
             ${CMAKE_CURRENT_SOURCE_DIR}/run-eds-test.sh ${DBUS_RUNNER} ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME} ${TESTNAME}
             ${EVOLUTION_CALENDAR_FACTORY} ${EVOLUTION_CALENDAR_SERVICE_NAME}
             ${EVOLUTION_SOURCE_REGISTRY}  ${EVOLUTION_SOURCE_SERVICE_NAME}
             ${GVFSD})
endmacro(declare_test testname)

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

declare_test(parseecal-test)
declare_test(collections-test)
declare_test(event-test)
declare_test(fetchitem-test)
declare_test(recurrence-test)
declare_test(cancel-operation-test)
declare_test(filter-test)
