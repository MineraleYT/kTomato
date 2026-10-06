# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
include(ECMAddTests)

set(CORE ${CMAKE_SOURCE_DIR}/src/core)
set(DB ${CMAKE_SOURCE_DIR}/src/db)
set(PLATFORM ${CMAKE_SOURCE_DIR}/src/platform)
set(STATS ${CMAKE_SOURCE_DIR}/src/stats)

# Core logic is compiled straight into each test; it has no dependency on the UI.
ecm_add_test(tst_timerengine.cpp
    ${CORE}/TimerEngine.cpp
    TEST_NAME tst_timerengine
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml
)

ecm_add_test(tst_focussoundcontroller.cpp
    ${CORE}/FocusSoundController.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/AppSettings.cpp
    ${PLATFORM}/Autostart.cpp
    ${PLATFORM}/SystemTray.cpp
    TEST_NAME tst_focussoundcontroller
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::DBus KF6::ConfigCore KF6::I18n KF6::ColorScheme
)

ecm_add_test(tst_datamanager.cpp
    tst_datamanager.cpp
    ${CORE}/DataManager.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    ${DB}/Database.cpp
    ${DB}/PresetRepository.cpp
    ${DB}/PresetPersistence.cpp
    ${DB}/SessionRepository.cpp
    ${STATS}/StatsModel.cpp
    ${STATS}/StatsAggregator.cpp
    ${STATS}/CsvExporter.cpp
    ${DB}/SessionRecorder.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/TimerBinding.cpp
    TEST_NAME tst_datamanager
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml Qt6::Sql KF6::I18n
)

ecm_add_test(tst_presetmodel.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    TEST_NAME tst_presetmodel
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml KF6::I18n
)

ecm_add_test(tst_timerbinding.cpp
    ${CORE}/TimerBinding.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    TEST_NAME tst_timerbinding
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml KF6::I18n
)

ecm_add_test(tst_database.cpp
    ${DB}/Database.cpp
    TEST_NAME tst_database
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Sql
)

ecm_add_test(tst_presetrepository.cpp
    ${DB}/Database.cpp
    ${DB}/PresetRepository.cpp
    ${DB}/PresetPersistence.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    TEST_NAME tst_presetrepository
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml Qt6::Sql KF6::I18n
)

ecm_add_test(tst_sessions.cpp
    ${DB}/Database.cpp
    ${DB}/PresetRepository.cpp
    ${DB}/SessionRepository.cpp
    ${DB}/SessionRecorder.cpp
    ${DB}/PresetPersistence.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/TimerBinding.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    TEST_NAME tst_sessions
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml Qt6::Sql KF6::I18n
)

ecm_add_test(tst_silence.cpp
    ${CORE}/SilenceController.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/TimerBinding.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    ${PLATFORM}/NotificationInhibitor.cpp
    TEST_NAME tst_silence
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml KF6::I18n
)

ecm_add_test(tst_freedesktopinhibitor.cpp
    ${PLATFORM}/FreedesktopInhibitor.cpp
    ${PLATFORM}/NotificationInhibitor.cpp
    TEST_NAME tst_freedesktopinhibitor
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::DBus
)

ecm_add_test(tst_powerinhibitor.cpp
    ${PLATFORM}/PowerInhibitor.cpp
    ${PLATFORM}/Autostart.cpp
    ${PLATFORM}/SystemTray.cpp
    ${CORE}/AppSettings.cpp
    ${CORE}/TimerEngine.cpp
    TEST_NAME tst_powerinhibitor
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::DBus KF6::ConfigCore KF6::I18n KF6::ColorScheme
)

ecm_add_test(tst_autostart.cpp
    ${PLATFORM}/Autostart.cpp
    TEST_NAME tst_autostart
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::DBus
)

ecm_add_test(tst_appsettings.cpp
    ${CORE}/AppSettings.cpp
    ${PLATFORM}/Autostart.cpp
    ${PLATFORM}/SystemTray.cpp
    TEST_NAME tst_appsettings
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::DBus KF6::ConfigCore KF6::I18n KF6::ColorScheme
)

ecm_add_test(tst_traypresenter.cpp
    ${CORE}/TrayPresenter.cpp
    ${CORE}/TimerEngine.cpp
    TEST_NAME tst_traypresenter
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml KF6::I18n
)

ecm_add_test(tst_singleinstance.cpp
    ${PLATFORM}/SingleInstance.cpp
    TEST_NAME tst_singleinstance
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::DBus
)

ecm_add_test(tst_statsaggregator.cpp
    ${STATS}/StatsAggregator.cpp
    ${DB}/SessionRepository.cpp
    TEST_NAME tst_statsaggregator
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Sql
)

ecm_add_test(tst_csvexporter.cpp
    ${STATS}/CsvExporter.cpp
    ${DB}/SessionRepository.cpp
    TEST_NAME tst_csvexporter
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Sql
)

ecm_add_test(tst_statsmodel.cpp
    ${STATS}/StatsModel.cpp
    ${STATS}/StatsAggregator.cpp
    ${STATS}/CsvExporter.cpp
    ${DB}/Database.cpp
    ${DB}/PresetRepository.cpp
    ${DB}/SessionRepository.cpp
    ${DB}/SessionRecorder.cpp
    ${DB}/PresetPersistence.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/TimerBinding.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    TEST_NAME tst_statsmodel
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml Qt6::Sql KF6::I18n
)

ecm_add_test(tst_krunnerservice.cpp
    ${PLATFORM}/KRunnerService.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    TEST_NAME tst_krunnerservice
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml Qt6::DBus KF6::I18n
)

ecm_add_test(tst_updatechecker.cpp
    ${CORE}/UpdateChecker.cpp
    TEST_NAME tst_updatechecker
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Qml Qt6::Network KF6::I18n
)

ecm_add_test(tst_screenlockwatcher.cpp
    ${PLATFORM}/ScreenLockWatcher.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/AppSettings.cpp
    ${PLATFORM}/Autostart.cpp
    ${PLATFORM}/SystemTray.cpp
    TEST_NAME tst_screenlockwatcher
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::DBus KF6::ConfigCore KF6::I18n KF6::ColorScheme
)

ecm_add_test(tst_diagnostics.cpp
    ${CORE}/Diagnostics.cpp
    ${CORE}/AppSettings.cpp
    ${PLATFORM}/Autostart.cpp
    ${PLATFORM}/SystemTray.cpp
    ${CORE}/DataManager.cpp
    ${DB}/Database.cpp
    ${CORE}/PresetModel.cpp
    ${CORE}/PresetSettings.cpp
    ${DB}/PresetRepository.cpp
    ${DB}/PresetPersistence.cpp
    ${DB}/SessionRepository.cpp
    ${STATS}/StatsModel.cpp
    ${STATS}/StatsAggregator.cpp
    ${STATS}/CsvExporter.cpp
    ${DB}/SessionRecorder.cpp
    ${CORE}/TimerEngine.cpp
    ${CORE}/TimerBinding.cpp
    TEST_NAME tst_diagnostics
    LINK_LIBRARIES Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Sql Qt6::DBus KF6::ConfigCore KF6::I18n KF6::ColorScheme
)
# The test builds a QGuiApplication (clipboard access): run it without a display, e.g. on CI.
set_tests_properties(tst_diagnostics PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

foreach(test tst_timerengine tst_focussoundcontroller tst_datamanager tst_presetmodel tst_timerbinding tst_database tst_presetrepository tst_sessions
             tst_silence tst_freedesktopinhibitor tst_autostart tst_appsettings tst_traypresenter
             tst_powerinhibitor tst_singleinstance tst_statsaggregator tst_csvexporter tst_statsmodel tst_krunnerservice tst_updatechecker tst_screenlockwatcher tst_diagnostics)
    target_include_directories(${test} PRIVATE ${CORE} ${DB} ${PLATFORM} ${STATS} ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

add_test(NAME tst_app_startup
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.sh $<TARGET_FILE:ktomato>
)
set_tests_properties(tst_app_startup PROPERTIES TIMEOUT 15)

add_test(NAME tst_stats_page_ui
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.sh $<TARGET_FILE:ktomato> stats
)
set_tests_properties(tst_stats_page_ui PROPERTIES TIMEOUT 10)

add_test(NAME tst_about_page_ui
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.sh $<TARGET_FILE:ktomato> about
)
set_tests_properties(tst_about_page_ui PROPERTIES TIMEOUT 10)

add_test(NAME tst_settings_page_ui
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.sh $<TARGET_FILE:ktomato> settings
)
set_tests_properties(tst_settings_page_ui PROPERTIES TIMEOUT 10)

add_test(NAME tst_presets_page_ui
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.sh $<TARGET_FILE:ktomato> presets
)
set_tests_properties(tst_presets_page_ui PROPERTIES TIMEOUT 10)

add_test(NAME tst_welcome_page_ui
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.sh $<TARGET_FILE:ktomato> welcome
)
set_tests_properties(tst_welcome_page_ui PROPERTIES TIMEOUT 10)
