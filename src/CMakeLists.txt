# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(ktomato main.cpp)

ecm_add_qml_module(ktomato
    URI "io.github.mineraleyt.ktomato"
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/TimerPage.qml
        qml/pages/PresetsPage.qml
        qml/pages/StatsPage.qml
        qml/pages/PresetEditPage.qml
        qml/pages/AppSettingsPage.qml
        qml/pages/AboutPage.qml
        qml/pages/WelcomePage.qml
        qml/components/ProgressRing.qml
)

# C++ types exposed to QML (QML_ELEMENT / QML_SINGLETON) live in core/.
target_sources(ktomato PRIVATE
    core/Clock.h
    core/TimerEngine.h
    core/TimerEngine.cpp
    core/TimerPreset.h
    core/PresetSettings.h
    core/PresetSettings.cpp
    core/PresetModel.h
    core/PresetModel.cpp
    core/TimerBinding.h
    core/TimerBinding.cpp
    core/PhaseNotifier.h
    core/PhaseNotifier.cpp
    core/FocusSoundController.h
    core/FocusSoundController.cpp
    core/DataManager.h
    core/DataManager.cpp
    core/SilenceController.h
    core/SilenceController.cpp
    core/AppSettings.h
    core/AppSettings.cpp
    core/TrayPresenter.h
    core/TrayPresenter.cpp
    core/TrayController.h
    core/TrayController.cpp
    core/UpdateChecker.h
    core/UpdateChecker.cpp
    core/Diagnostics.h
    core/Diagnostics.cpp
    platform/NotificationInhibitor.h
    platform/NotificationInhibitor.cpp
    platform/PowerInhibitor.h
    platform/PowerInhibitor.cpp
    platform/NoopInhibitor.h
    platform/FreedesktopInhibitor.h
    platform/FreedesktopInhibitor.cpp
    platform/InhibitorFactory.h
    platform/InhibitorFactory.cpp
    platform/UnixSignals.h
    platform/UnixSignals.cpp
    platform/Autostart.h
    platform/Autostart.cpp
    platform/SystemTray.h
    platform/SystemTray.cpp
    platform/SingleInstance.h
    platform/SingleInstance.cpp
    platform/DBusTimerService.h
    platform/DBusTimerService.cpp
    platform/KRunnerService.h
    platform/KRunnerService.cpp
    platform/ScreenLockWatcher.h
    platform/ScreenLockWatcher.cpp
    db/Database.h
    db/Database.cpp
    db/PresetRepository.h
    db/PresetRepository.cpp
    db/PresetPersistence.h
    db/PresetPersistence.cpp
    db/SessionRepository.h
    db/SessionRepository.cpp
    db/SessionRecorder.h
    db/SessionRecorder.cpp
    stats/StatsTypes.h
    stats/StatsAggregator.h
    stats/StatsAggregator.cpp
    stats/CsvExporter.h
    stats/CsvExporter.cpp
    stats/StatsModel.h
    stats/StatsModel.cpp
)
target_include_directories(ktomato PRIVATE core db platform stats)

# Embedded fallback so the icon also shows when running from the build tree.
qt_add_resources(ktomato "appicon"
    PREFIX "/icons"
    BASE "${CMAKE_SOURCE_DIR}/data/icons/hicolor/scalable/apps"
    FILES "${CMAKE_SOURCE_DIR}/data/icons/hicolor/scalable/apps/io.github.mineraleyt.ktomato.svg"
)

target_compile_definitions(ktomato PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

target_link_libraries(ktomato PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::DBus
    Qt6::Network
    KF6::Kirigami
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::ColorScheme
    KF6::Notifications
    KF6::ConfigCore
    KF6::StatusNotifierItem
)

install(TARGETS ktomato ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
