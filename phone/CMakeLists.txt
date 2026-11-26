# LioraPhone: the QML phone prototype on LioraCore
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Svg)

if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)                       # QML files under :/qt/qml, where the engine looks
endif()

qt_add_executable(LioraPhone
    WIN32 MACOSX_BUNDLE
    main.cpp
    demodata.cpp demodata.h
)

qt_add_qml_module(LioraPhone
    URI LioraPhone
    VERSION 1.0
    QML_FILES
        Main.qml
        OverviewPage.qml
        TransactionsPage.qml
        PlaceholderPage.qml
        Card.qml
        CategoryTag.qml
        TabBarButton.qml
    SOURCES
        phonebackend.cpp phonebackend.h
)

# monochrome icons from the desktop app (:/icons/<name>.svg); QML tints them
file(GLOB PHONE_ICON_FILES "${PROJECT_SOURCE_DIR}/assets/icons/*.svg")
qt_add_resources(LioraPhone "phone_icons"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/assets"
    FILES ${PHONE_ICON_FILES}
)

target_link_libraries(LioraPhone PRIVATE
    LioraCore
    Qt::Quick
    Qt::QuickControls2
    Qt::Svg                                          # The SVG image plugin for the icons
)

# iOS: the app's ID on the device.
# TODO: Change prefix before signing
set_target_properties(LioraPhone PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER org.leinehive.liora.phone
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER org.leinehive.liora.phone
    MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
)
