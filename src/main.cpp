// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QQmlContext>

#include <memory>

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "AppSettings.h"
#include "DataManager.h"
#include "Database.h"
#include "DBusTimerService.h"
#include "KRunnerService.h"
#include "InhibitorFactory.h"
#include "FocusSoundController.h"
#include "PhaseNotifier.h"
#include "PresetModel.h"
#include "PresetPersistence.h"
#include "PresetRepository.h"
#include "SessionRecorder.h"
#include "SessionRepository.h"
#include "SilenceController.h"
#include "SingleInstance.h"
#include "StatsModel.h"
#include "TimerBinding.h"
#include "TimerEngine.h"
#include "TrayController.h"
#include "UnixSignals.h"
#include "PowerInhibitor.h"
#include "ScreenLockWatcher.h"
#include "Diagnostics.h"

int main(int argc, char *argv[])
{
    Diagnostics::installLogHandler();

    // Native KDE look unless the user (or a non-KDE desktop) chose a style.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QApplication app(argc, argv);

    // Breeze widget style whatever the platform; the colour scheme follows the user's
    // setting (applied once AppSettings exists, below).
    QApplication::setStyle(QStringLiteral("breeze"));

    KLocalizedString::setApplicationDomain("ktomato");
    const QString devLocaleDir = QCoreApplication::applicationDirPath() + QStringLiteral("/../share/locale");
    if (QDir(devLocaleDir).exists()) {
        KLocalizedString::addDomainLocaleDir("ktomato", devLocaleDir);
    }
    const QString userLocaleDir = QDir::homePath() + QStringLiteral("/.local/share/locale");
    if (QDir(userLocaleDir).exists()) {
        KLocalizedString::addDomainLocaleDir("ktomato", userLocaleDir);
    }
    // The chosen language must be active before the first i18n() call (about data, default
    // preset names created on first run).
    AppSettings::applyStoredLanguage();
    QCoreApplication::setOrganizationName(QStringLiteral("mineraleyt"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("io.github.mineraleyt"));
    QCoreApplication::setApplicationName(QStringLiteral("ktomato"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.mineraleyt.ktomato"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.mineraleyt.ktomato"),
                                                    QIcon(QStringLiteral(":/icons/io.github.mineraleyt.ktomato.svg"))));

    KAboutData about(QStringLiteral("ktomato"),
                     i18n("kTomato"),
                     QStringLiteral(PROJECT_VERSION),
                     i18n("A Pomodoro timer with time statistics"),
                     KAboutLicense::GPL_V3,
                     i18n("© 2026 kTomato contributors"));
    about.setDesktopFileName(QStringLiteral("io.github.mineraleyt.ktomato"));
    about.setProgramLogo(QStringLiteral("io.github.mineraleyt.ktomato"));
    about.setHomepage(QStringLiteral("https://github.com/MineraleYT/kTomato"));
    about.setBugAddress(QByteArrayLiteral("https://github.com/MineraleYT/kTomato/issues"));
    about.addAuthor(i18n("MinerAle"), i18n("Maintainer & Developer"), QStringLiteral(""));
    about.addCredit(i18n("KDE Frameworks"), i18n("Libraries and integration APIs used by this application"));
    // setApplicationData() also sets the organization domain from the about data; without this
    // it would replace ours with KAboutData's default.
    about.setOrganizationDomain(QByteArrayLiteral("io.github.mineraleyt"));
    KAboutData::setApplicationData(about);

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    const QCommandLineOption backgroundOption(QStringLiteral("background"),
                                              i18n("Start hidden in the system tray (used by the login entry)"));
    parser.addOption(backgroundOption);
    parser.process(app);
    about.processCommandLine(&parser);
    const bool background = parser.isSet(backgroundOption);

    // Closing the window must not end the program: the tray (or the close handler in QML) decides.
    app.setQuitOnLastWindowClosed(false);

    // One instance only. A second launch (login entry plus menu, say) shows the first one's
    // window and exits, so two programs never share the database. A second `--background`
    // launch (the login entry while already running) exits without popping the window up.
    SingleInstance singleInstance(QDBusConnection::sessionBus(), QStringLiteral("io.github.mineraleyt.ktomato"));
    if (!singleInstance.acquire(!background)) {
        return EXIT_SUCCESS;
    }

    // Lifetimes: objects are destroyed in reverse order of declaration. The database, the
    // repositories and the notification inhibitor are declared before the QML engine (whose
    // singletons, e.g. SilenceController and StatsModel, use them), and the helpers parented to
    // `services` after it, so each object goes away before what it points to.

    // Storage. If the file cannot be used, run with a throw-away in-memory database so the
    // timer still works; nothing is saved in that case (the UI shows a warning).
    auto database = std::make_unique<Database>(Database::defaultPath());
    if (!database->open()) {
        qWarning() << "Cannot open the database" << Database::defaultPath() << ":" << database->lastError();
        qWarning() << "Using a temporary in-memory database: timers and statistics will NOT be saved";
        database = std::make_unique<Database>(QStringLiteral(":memory:"));
        if (!database->open()) {
            qCritical() << "Cannot open even an in-memory database:" << database->lastError();
            return EXIT_FAILURE;
        }
    }
    if (!database->movedAsidePath().isEmpty()) {
        if (database->movedAsideWasCorrupt()) {
            qWarning() << "The existing database was damaged; it was kept as" << database->movedAsidePath()
                       << "and a new one was started";
        } else {
            qWarning() << "The existing database was not created by kTomato; it was kept as"
                       << database->movedAsidePath() << "and a new one was started";
        }
    }
    PresetRepository presetRepository(database->connectionName());
    SessionRepository sessionRepository(database->connectionName());
    const auto inhibitor = createNotificationInhibitor();

    QQmlApplicationEngine engine;
    QObject services; // parent of the C++ helpers below; destroyed before the engine
    KLocalization::setupLocalizedContext(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("appAboutData"), QVariant::fromValue(about));
    const QString smokePage = qEnvironmentVariable("KTOMATO_SMOKE_PAGE");
    const bool statsSmokeTest = qEnvironmentVariable("KTOMATO_STATS_SMOKE_TEST") == QStringLiteral("1");
    engine.rootContext()->setContextProperty(QStringLiteral("appStatsSmokeTest"), statsSmokeTest);
    engine.rootContext()->setContextProperty(QStringLiteral("appSmokePage"), smokePage);
    if (statsSmokeTest || !smokePage.isEmpty()) {
        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
            for (const QQmlError &warning : warnings) {
                qWarning().noquote() << warning.toString();
            }
        });
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);

    // Services that must outlive any page. They are wired before the UI loads so the
    // first frame already shows the stored timers. Order matters: load presets first,
    // then configure the engine from them, then start recording.
    auto *timer = engine.singletonInstance<TimerEngine *>("io.github.mineraleyt.ktomato", "TimerEngine");
    auto *presets = engine.singletonInstance<PresetModel *>("io.github.mineraleyt.ktomato", "PresetModel");
    auto *settings = engine.singletonInstance<AppSettings *>("io.github.mineraleyt.ktomato", "AppSettings");
    if (!timer || !presets || !settings) {
        qCritical() << "Cannot create the QML singletons";
        return EXIT_FAILURE;
    }
    settings->setLaunchedInBackground(background);
    settings->applyLanguage();
    settings->applyColorScheme();
    auto *presetPersistence = new PresetPersistence(presets, &presetRepository, &services);
    // Only mark the starter timers as created if they can really be saved; otherwise they would
    // never be offered once the database reads fine again.
    if (presetPersistence->isActive() && !presetRepository.presetsInitialized()) {
        if (presets->count() == 1 && presets->currentUuid() == PresetModel::DefaultUuid) {
            presets->addStandardPresets();
        }
        presetRepository.setPresetsInitialized(true);
    }
    new TimerBinding(timer, presets, &services);
    auto *silence = engine.singletonInstance<SilenceController *>("io.github.mineraleyt.ktomato", "SilenceController");
    if (!silence) {
        qCritical() << "Cannot create the QML singletons";
        return EXIT_FAILURE;
    }
    silence->attach(timer, presets, inhibitor.get());
    new DBusTimerService(timer, timer);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Timer"), timer, QDBusConnection::ExportAdaptors);
    auto *krunner = new KRunnerService(timer, presets, &services);
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/krunner"), krunner, QDBusConnection::ExportAllSlots);
    auto *recorder = new SessionRecorder(timer, presets, &sessionRepository, &services);

    auto *stats = engine.singletonInstance<StatsModel *>("io.github.mineraleyt.ktomato", "StatsModel");
    if (!stats) {
        qCritical() << "Cannot create the StatsModel singleton";
        return EXIT_FAILURE;
    }
    stats->attach(&sessionRepository, recorder);
    // StatsModel read QLocale() when it was created; make sure it sees the applied language.
    stats->refreshEnvironment();
    auto *dataManager = new DataManager(database.get(), &sessionRepository, &presetRepository, presets, stats, &services);
    dataManager->setSessionRecorder(recorder);
    engine.rootContext()->setContextProperty(QStringLiteral("DataManager"), dataManager);

    QObject::connect(settings, &AppSettings::languageChanged, &engine, &QQmlApplicationEngine::retranslate);
    QObject::connect(settings, &AppSettings::languageChanged, stats, &StatsModel::refreshEnvironment);
    auto *notifier = new PhaseNotifier(timer, presets, inhibitor.get(), settings, &services);
    notifier->setLastWorkSessionIdProvider([recorder]() { return recorder->lastWorkSessionId(); });
    // Direct inline reply from the notification.
    QObject::connect(notifier, &PhaseNotifier::taskNoteSubmitted, dataManager, [dataManager](qint64 sessionId, const QString &note) {
        if (sessionId > 0) {
            dataManager->updateSessionNote(sessionId, note);
        }
    });
    auto *powerInhibitor = new PowerInhibitor(QDBusConnection::sessionBus(), &services);
    powerInhibitor->attach(timer, settings);
    auto *screenLockWatcher = engine.singletonInstance<ScreenLockWatcher *>("io.github.mineraleyt.ktomato", "ScreenLockWatcher");
    if (screenLockWatcher) {
        screenLockWatcher->attach(timer, settings);
    }
    auto *diagnostics = engine.singletonInstance<Diagnostics *>("io.github.mineraleyt.ktomato", "Diagnostics");
    if (diagnostics) {
        diagnostics->attach(settings, dataManager);
    }
    auto *focusSound = new FocusSoundController(&services);
    focusSound->attach(timer, settings);
    engine.rootContext()->setContextProperty(QStringLiteral("FocusSoundController"), focusSound);
    stats->setDailyGoal(settings->dailyGoal());
    stats->setProtectWeekendStreak(settings->protectWeekendStreak());
    QObject::connect(settings, &AppSettings::dailyGoalChanged, stats, [settings, stats]() {
        stats->setDailyGoal(settings->dailyGoal());
    });
    QObject::connect(settings, &AppSettings::protectWeekendStreakChanged, stats, [settings, stats]() {
        stats->setProtectWeekendStreak(settings->protectWeekendStreak());
    });
    auto *tray = new TrayController(timer, presets, settings, &services);

    // Quitting mid-phase ends it as an interrupted session instead of losing it.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, timer, &TimerEngine::stop);
    // ... and always hands the notifications back before the process ends.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, silence, &SilenceController::shutdown);
    installQuitSignalHandlers(&app);

    engine.loadFromModule("io.github.mineraleyt.ktomato", "Main");

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (window) {
        tray->setWindow(window);
        auto activateWindow = [window](const QString &activationToken = QString()) {
            // On Wayland the compositor only gives focus with the token of the launch that asked
            // for it; Qt's Wayland plugin takes it from this variable on requestActivate().
            if (!activationToken.isEmpty()) {
                qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
            }
            window->show();
            if (window->windowStates() & Qt::WindowMinimized) {
                window->showNormal();
            }
            window->raise();
            window->requestActivate();
        };

        // A second launch asks this instance to come to the front.
        QObject::connect(&singleInstance, &SingleInstance::activateRequested, window, activateWindow);

        // KRunner runner action to open window
        QObject::connect(krunner, &KRunnerService::openRequested, window, [activateWindow]() { activateWindow(); });

        // A click on a notification with nothing more specific to do.
        QObject::connect(notifier, &PhaseNotifier::showWindowRequested, window, [activateWindow]() { activateWindow(); });

        // When a task note prompt is requested (e.g. from end-of-work notification action)
        QObject::connect(notifier, &PhaseNotifier::taskNotePromptRequested, window,
                         [window, activateWindow](const QString &presetName, qint64 sessionId) {
                             activateWindow();
                             QMetaObject::invokeMethod(window, "openTaskNotePrompt", Q_ARG(QVariant, presetName),
                                                       Q_ARG(QVariant, sessionId));
                         });
    }

    return app.exec();
}
