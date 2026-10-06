// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "FakeClock.h"
#include "FocusSoundController.h"
#include "TimerEngine.h"

class FocusSoundControllerTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;
    QByteArray oldPath;
    QByteArray oldCapture;
    FakeClock clock;
    std::unique_ptr<TimerEngine> engine;
    std::unique_ptr<FocusSoundController> sounds;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(dir.isValid());
        const QString bin = dir.filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(bin));
        QFile player(bin + QStringLiteral("/pw-play"));
        QVERIFY(player.open(QIODevice::WriteOnly));
        player.write("#!/bin/sh\ncat > \"$FOCUS_SOUND_CAPTURE\"\n");
        player.close();
        QVERIFY(player.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        oldPath = qgetenv("PATH");
        oldCapture = qgetenv("FOCUS_SOUND_CAPTURE");
        QByteArray path = bin.toLocal8Bit();
        path.append(':');
        path.append(oldPath);
        qputenv("PATH", path);
        qputenv("FOCUS_SOUND_CAPTURE", dir.filePath(QStringLiteral("audio.raw")).toLocal8Bit());
    }

    void cleanupTestCase()
    {
        qputenv("PATH", oldPath);
        if (oldCapture.isNull()) qunsetenv("FOCUS_SOUND_CAPTURE");
        else qputenv("FOCUS_SOUND_CAPTURE", oldCapture);
    }

    void init()
    {
        QFile::remove(dir.filePath(QStringLiteral("audio.raw")));
        clock = FakeClock();
        engine = std::make_unique<TimerEngine>(&clock, nullptr);
        sounds = std::make_unique<FocusSoundController>();
        sounds->attach(engine.get(), nullptr);
        sounds->setMode(static_cast<int>(FocusSoundController::Mode::WhiteNoise));
        sounds->setVolume(45);
    }

    void cleanup()
    {
        sounds.reset();
        engine.reset();
    }

    void audioStartsForWorkAndStopsWhenPaused()
    {
        engine->start();
        QTRY_VERIFY_WITH_TIMEOUT(sounds->isPlaying(), 3000);
        const QString capture = dir.filePath(QStringLiteral("audio.raw"));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(capture).size() > 0, 3000);

        engine->pause();
        QTRY_VERIFY(!sounds->isPlaying());
        engine->resume();
        QTRY_VERIFY_WITH_TIMEOUT(sounds->isPlaying(), 3000);
        engine->stop();
        QTRY_VERIFY(!sounds->isPlaying());
    }

    void changingModeOffStopsPlayback()
    {
        engine->start();
        QTRY_VERIFY_WITH_TIMEOUT(sounds->isPlaying(), 3000);
        sounds->setMode(static_cast<int>(FocusSoundController::Mode::Off));
        QTRY_VERIFY(!sounds->isPlaying());
    }

    void changingModeWhilePlayingKeepsThePlayer()
    {
        engine->start();
        QTRY_VERIFY_WITH_TIMEOUT(sounds->isPlaying(), 3000);
        QSignalSpy playing(sounds.get(), &FocusSoundController::isPlayingChanged);
        sounds->setMode(static_cast<int>(FocusSoundController::Mode::Rain));
        engine->pause();
        engine->resume();
        QTRY_VERIFY_WITH_TIMEOUT(sounds->isPlaying(), 3000);
        sounds->setMode(static_cast<int>(FocusSoundController::Mode::ClockTick));
        QTest::qWait(100);
        QVERIFY(sounds->isPlaying());
        QCOMPARE(playing.count(), 2); // only the pause/resume, not the mode changes
    }

    void keepsHalfASecondBufferedAhead()
    {
        engine->start();
        const QString capture = dir.filePath(QStringLiteral("audio.raw"));
        // 500 ms of mono s16 at 44.1 kHz is 44100 bytes, written right at the start.
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(capture).size() >= 44100, 3000);
    }

    void invalidModesAreClamped()
    {
        sounds->setMode(9);
        QCOMPARE(sounds->mode(), 3);
        sounds->setMode(-2);
        QCOMPARE(sounds->mode(), 0);
    }
};

QTEST_GUILESS_MAIN(FocusSoundControllerTest)
#include "tst_focussoundcontroller.moc"
