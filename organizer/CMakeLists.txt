project(qorganizer-eds-src)

set(QORGANIZER_BACKEND qtorganizer_eds)

set(QORGANIZER_BACKEND_SRCS
    qorganizer-eds-collection-engineid.cpp
    qorganizer-eds-fetchrequestdata.cpp
    qorganizer-eds-fetchbyidrequestdata.cpp
    qorganizer-eds-fetchocurrencedata.cpp
    qorganizer-eds-engine.cpp
    qorganizer-eds-enginedata.cpp
    qorganizer-eds-engineid.cpp
    qorganizer-eds-parseeventthread.cpp
    qorganizer-eds-removecollectionrequestdata.cpp
    qorganizer-eds-removerequestdata.cpp
    qorganizer-eds-removebyidrequestdata.cpp
    qorganizer-eds-requestdata.cpp
    qorganizer-eds-savecollectionrequestdata.cpp
    qorganizer-eds-saverequestdata.cpp
    qorganizer-eds-viewwatcher.cpp
    qorganizer-eds-source-registry.cpp
)

set(QORGANIZER_BACKEND_HDRS
    qorganizer-eds-collection-engineid.h
    qorganizer-eds-fetchrequestdata.h
    qorganizer-eds-fetchbyidrequestdata.h
    qorganizer-eds-fetchocurrencedata.h
    qorganizer-eds-engine.h
    qorganizer-eds-enginedata.h
    qorganizer-eds-engineid.h
    qorganizer-eds-parseeventthread.h
    qorganizer-eds-removecollectionrequestdata.h
    qorganizer-eds-removerequestdata.h
    qorganizer-eds-removebyidrequestdata.h
    qorganizer-eds-requestdata.h
    qorganizer-eds-savecollectionrequestdata.h
    qorganizer-eds-saverequestdata.h
    qorganizer-eds-source-registry.h
    qorganizer-eds-viewwatcher.h
)

add_library(${QORGANIZER_BACKEND}-lib STATIC
    ${QORGANIZER_BACKEND_SRCS}
    ${QORGANIZER_BACKEND_HDRS}
)

add_library(${QORGANIZER_BACKEND} MODULE
    qorganizer-eds-factory.cpp
    qorganizer-eds-factory.h
)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${ECAL_INCLUDE_DIRS}
    ${EDATASERVER_INCLUDE_DIRS}
    ${UEDS_INCLIDE_DIRS}
)

target_link_libraries(${QORGANIZER_BACKEND}-lib
    ${GLIB_LIBRARIES}
    ${ECAL_LIBRARIES}
    ${EDATASERVER_LIBRARIES}
    ${UEDS_LIBRARIES}
)

target_link_libraries(${QORGANIZER_BACKEND}
    ${QORGANIZER_BACKEND}-lib
    ${GLIB_LIBRARIES}
    ${ECAL_LIBRARIES}
    ${EDATASERVER_LIBRARIES}
    ${UEDS_LIBRARIES}
)

qt5_use_modules(${QORGANIZER_BACKEND}-lib Core Organizer)
qt5_use_modules(${QORGANIZER_BACKEND} Core Organizer)

execute_process(
    COMMAND qmake -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_INSTALL_PLUGINS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS ${QORGANIZER_BACKEND} LIBRARY DESTINATION ${QT_INSTALL_PLUGINS}/organizer)
