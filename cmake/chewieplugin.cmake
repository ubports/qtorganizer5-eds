# -*- cmake -*-
# Generates a rule to create chewie plugin
#
# Typical use -
#
# SET(SRC_FILES head1.h head2.h head3.h)
# SET(LIBRARIES foolib barlib)
# SET(QT_MODULES Core Qml)
# SET(EXTRA_INCLUDE ${GLIB_INCLUDE_DIRS})
# CREATE_CHEWIE_PLUGIN(fooplugin LIBRARIES QT_MODULES EXTRA_INCLUDE SRC_FILES)
macro(CREATE_CHEWIE_PLUGIN PLUGIN_NAME PLUGIN_LINK_LIBRARIES PLUGIN_QT_MODULES PLUGIN_EXTRA_INCLUDE PLUGIN_SOURCE)
    add_library(${PLUGIN_NAME} MODULE
                ${${PLUGIN_SOURCE}})

    set_target_properties(${PLUGIN_NAME}
                          PROPERTIES PREFIX ""
                          LIBRARY_OUTPUT_DIRECTORY ${chewieplugins_BINARY_DIR})

    target_link_libraries(${PLUGIN_NAME}
                          ${${PLUGIN_LINK_LIBRARIES}})

    qt5_use_modules(${PLUGIN_NAME} ${${PLUGIN_QT_MODULES}})

    include_directories(
            ${libchewieui_SOURCE_DIR}
            ${${PLUGIN_EXTRA_INCLUDE}})

    install(TARGETS ${PLUGIN_NAME}
            LIBRARY DESTINATION ${CHEWIE_PLUGINS_DIR})

endmacro()
