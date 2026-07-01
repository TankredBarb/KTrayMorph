qt_add_qml_module(ktraymorphcore
    URI org.ktraymorph.core
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core
    SOURCES
        ruleengine.cpp
        ruleengine.h
        logging.cpp
        logging.h
        statusnotifierscanner.cpp
        statusnotifierscanner.h
        trayitem.h
        trayitemmodel.cpp
        trayitemmodel.h
        livereplacementcontroller.cpp
        livereplacementcontroller.h
)

target_link_libraries(ktraymorphcore
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::DBus
        Qt6::Concurrent
        KF6::ConfigCore
        KF6::I18n
        KF6::IconThemes
)

install(TARGETS ktraymorphcore ktraymorphcoreplugin
    LIBRARY DESTINATION ${KDE_INSTALL_QMLDIR}/org/ktraymorph/core
    RUNTIME DESTINATION ${KDE_INSTALL_QMLDIR}/org/ktraymorph/core
)

install(FILES
    ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core/qmldir
    ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core/ktraymorphcore.qmltypes
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/ktraymorph/core
)

install(FILES
    ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core/ktraymorphcoreplugin.so
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/ktraymorph/core
    RENAME libktraymorphcoreplugin.so
)

install(TARGETS ktraymorphcore ktraymorphcoreplugin
    LIBRARY DESTINATION ${KDE_INSTALL_DATADIR}/QtProject/qml/org/ktraymorph/core
    RUNTIME DESTINATION ${KDE_INSTALL_DATADIR}/QtProject/qml/org/ktraymorph/core
)

install(FILES
    ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core/qmldir
    ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core/ktraymorphcore.qmltypes
    DESTINATION ${KDE_INSTALL_DATADIR}/QtProject/qml/org/ktraymorph/core
)

install(FILES
    ${CMAKE_BINARY_DIR}/qml/org/ktraymorph/core/ktraymorphcoreplugin.so
    DESTINATION ${KDE_INSTALL_DATADIR}/QtProject/qml/org/ktraymorph/core
    RENAME libktraymorphcoreplugin.so
)

add_executable(ktraymorph-dump-sni
    dump_sni.cpp
    ruleengine.cpp
    ruleengine.h
    logging.cpp
    logging.h
    statusnotifierscanner.cpp
    statusnotifierscanner.h
    trayitem.h
    trayitemmodel.cpp
    trayitemmodel.h
)

target_link_libraries(ktraymorph-dump-sni
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::DBus
        Qt6::Concurrent
        KF6::IconThemes
)
