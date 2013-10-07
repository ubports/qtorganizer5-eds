project(qorganizer-eds-src)

set(QORGANIZER_BACKEND qtorganizer_eds)

set(QORGANIZER_BACKEND_SRCS
    qorganizer-eds-collection-engineid.cpp
    qorganizer-eds-fetchrequestdata.cpp
    qorganizer-eds-engine.cpp
    qorganizer-eds-engineid.cpp
    qorganizer-eds-removecollectionrequestdata.cpp
    qorganizer-eds-removerequestdata.cpp
    qorganizer-eds-requestdata.cpp
    qorganizer-eds-savecollectionrequestdata.cpp
    qorganizer-eds-saverequestdata.cpp
    qorganizer-eds-viewwatcher.cpp
)

set(QORGANIZER_BACKEND_HDRS
    qorganizer-eds-collection-engineid.h
    qorganizer-eds-fetchrequestdata.h
    qorganizer-eds-engine.h
    qorganizer-eds-engineid.h
    qorganizer-eds-removecollectionrequestdata.h
    qorganizer-eds-removerequestdata.h
    qorganizer-eds-requestdata.h
    qorganizer-eds-savecollectionrequestdata.h
    qorganizer-eds-saverequestdata.h
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
    ${CMAKE_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${ECAL_INCLUDE_DIRS}
    ${EDATASERVER_INCLUDE_DIRS}
)

target_link_libraries(${QORGANIZER_BACKEND}-lib
    ${GLIB_LIBRARIES}
    ${ECAL_LIBRARIES}
    ${EDATASERVER_LIBRARIES}
)

target_link_libraries(${QORGANIZER_BACKEND}
    ${QORGANIZER_BACKEND}-lib
    ${GLIB_LIBRARIES}
    ${ECAL_LIBRARIES}
    ${EDATASERVER_LIBRARIES}
)

qt5_use_modules(${QORGANIZER_BACKEND}-lib Core Organizer)
qt5_use_modules(${QORGANIZER_BACKEND} Core Organizer)

execute_process(
    COMMAND qmake -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE QT_INSTALL_PLUGINS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

install(TARGETS ${QORGANIZER_BACKEND} LIBRARY DESTINATION ${QT_INSTALL_PLUGINS}/organizer)
