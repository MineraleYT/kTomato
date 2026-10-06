# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)

project(kTomato VERSION 1.0.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.6.0)
set(KF_MIN_VERSION 6.8.0)  # KLocalization::setupLocalizedContext

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQmlModule)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
    PROJECT_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_USE_QSTRINGBUILDER
    QT_DISABLE_DEPRECATED_BEFORE=0x060600
)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Sql DBus Network)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Kirigami I18n CoreAddons ColorScheme Notifications Config StatusNotifierItem)

# Qt >= 6.8: one qmldir per QML subdirectory (silences the QTP0004 warning).
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

add_subdirectory(src)
ki18n_install(po)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(FILES data/io.github.mineraleyt.ktomato.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# KF6 KRunner only looks for D-Bus runners in share/krunner/dbusplugins.
install(FILES data/io.github.mineraleyt.ktomato.runner.desktop DESTINATION ${KDE_INSTALL_DATADIR}/krunner/dbusplugins)
# D-Bus activation: KRunner or a `qdbus` call can start kTomato (hidden in the tray) on demand.
configure_file(data/io.github.mineraleyt.ktomato.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.mineraleyt.ktomato.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.mineraleyt.ktomato.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES data/io.github.mineraleyt.ktomato.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES data/ktomato.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install(FILES data/icons/hicolor/scalable/apps/io.github.mineraleyt.ktomato.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
