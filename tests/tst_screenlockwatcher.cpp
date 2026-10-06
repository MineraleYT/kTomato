// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDBusConnection>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <memory>

#include "AppSettings.h"
#include "Autostart.h"
#include "FakeClock.h"
#include "ScreenLockWatcher.h"
#include "TimerEngine.h"

class ScreenLockWatcherTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<FakeClock> clock;
    std::unique_ptr<TimerEngine> engine;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<ScreenLockWatcher> watcher;

private Q_SLOTS:
    void initTestCase()
    {
        // Keep the settings file away from the user's real configuration.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        clock = std::make_unique<FakeClock>();
        engine = std::make_unique<TimerEngine>(clock.get(), nullptr);
        settings = std::make_unique<AppSettings>(KSharedConfig::openConfig(), nullptr, false);
        settings->setAutoPauseOnScreenLock(true);

        // Disconnected bus so tests don't touch system D-Bus
        QDBusConnection bus(QStringLiteral("tst_disconnected_bus"));
        watcher = std::make_unique<ScreenLockWatcher>(bus);
        watcher->attach(engine.get(), settings.get());
    }

    void lockPausesActiveWorkTimer()
    {
        engine->start();
        QCOMPARE(engine->state(), TimerEngine::State::Working);

        QSignalSpy pauseSpy(watcher.get(), &ScreenLockWatcher::pausedByScreenLockChanged);
        QSignalSpy unlockSpy(watcher.get(), &ScreenLockWatcher::screenUnlockedAfterPause);

        // Screen is locked
        watcher->onActiveChanged(true);

        QCOMPARE(engine->state(), TimerEngine::State::Paused);
        QVERIFY(watcher->isPausedByScreenLock());
        QCOMPARE(pauseSpy.count(), 1);

        // Screen is unlocked
        watcher->onActiveChanged(false);
        QCOMPARE(unlockSpy.count(), 1);
        QVERIFY(watcher->isPausedByScreenLock());

        // Resuming clears the flag
        engine->resume();
        QCOMPARE(engine->state(), TimerEngine::State::Working);
        QVERIFY(!watcher->isPausedByScreenLock());
    }

    void disabledSettingDoesNotPause()
    {
        settings->setAutoPauseOnScreenLock(false);
        engine->start();
        QCOMPARE(engine->state(), TimerEngine::State::Working);

        watcher->onActiveChanged(true);

        QCOMPARE(engine->state(), TimerEngine::State::Working);
        QVERIFY(!watcher->isPausedByScreenLock());
    }

    void lockWhileAlreadyPausedDoesNotMarkScreenLock()
    {
        engine->start();
        engine->pause();
        QCOMPARE(engine->state(), TimerEngine::State::Paused);

        watcher->onActiveChanged(true);
        QVERIFY(!watcher->isPausedByScreenLock());
    }

    void theSameChangeReportedTwiceCountsOnce()
    {
        engine->start();
        QSignalSpy pauseSpy(watcher.get(), &ScreenLockWatcher::pausedByScreenLockChanged);
        QSignalSpy unlockSpy(watcher.get(), &ScreenLockWatcher::screenUnlockedAfterPause);

        // Plasma sends ActiveChanged on /ScreenSaver and /org/freedesktop/ScreenSaver.
        watcher->onActiveChanged(true);
        watcher->onActiveChanged(true);
        QCOMPARE(pauseSpy.count(), 1);

        watcher->onActiveChanged(false);
        watcher->onActiveChanged(false);
        QCOMPARE(unlockSpy.count(), 1);
    }

    void clearNoticeClearsFlag()
    {
        engine->start();
        watcher->onActiveChanged(true);
        QVERIFY(watcher->isPausedByScreenLock());

        watcher->clearLockPauseNotice();
        QVERIFY(!watcher->isPausedByScreenLock());
    }
};

QTEST_GUILESS_MAIN(ScreenLockWatcherTest)
#include "tst_screenlockwatcher.moc"
