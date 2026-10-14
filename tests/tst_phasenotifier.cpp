// SPDX-License-Identifier: GPL-3.0-or-later
#include <KLocalizedString>

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <memory>

#include "FakeClock.h"
#include "FakeInhibitor.h"
#include "PhaseNotifier.h"
#include "PresetModel.h"
#include "TimerEngine.h"

/// The end-of-phase announcement depends on the desktop handing notifications back
/// (NotificationInhibitor::whenReleased). These tests drive that dependency end to end.
class PhaseNotifierTest : public QObject
{
    Q_OBJECT

private:
    FakeClock clock;
    std::unique_ptr<TimerEngine> engine;
    std::unique_ptr<PresetModel> presets;
    std::unique_ptr<FakeInhibitor> inhibitor;
    std::unique_ptr<PhaseNotifier> notifier;

    /// Ends a work phase while notifications are silenced and their release is in flight.
    void endWorkWhileSilenced()
    {
        engine->start();
        inhibitor->inhibit(QStringLiteral("focus"));
        clock.advanceSeconds(25 * 60);
        // What SilenceController does as the phase ends: ask for the release.
        inhibitor->release();
        engine->refresh();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("ktomato");
    }

    void init()
    {
        clock = FakeClock();
        engine = std::make_unique<TimerEngine>(&clock, nullptr);
        presets = std::make_unique<PresetModel>();
        inhibitor = std::make_unique<FakeInhibitor>();
        notifier = std::make_unique<PhaseNotifier>(engine.get(), presets.get(), inhibitor.get());
        notifier->setReleaseTimeoutMs(300);
    }

    void cleanup()
    {
        notifier.reset();
        inhibitor.reset();
        presets.reset();
        engine.reset();
    }

    void aReleaseThatNeverCompletesDoesNotSwallowTheAnnouncement()
    {
        inhibitor->deferReleases = true; // the desktop never confirms
        QSignalSpy announced(notifier.get(), &PhaseNotifier::phaseEndAnnounced);
        QElapsedTimer waited;
        waited.start();
        endWorkWhileSilenced();
        QVERIFY(inhibitor->isInhibited());

        // Held back while the release may still come...
        QTest::qWait(100);
        QCOMPARE(announced.count(), 0);
        // ...and sent anyway once the timeout is over.
        QTRY_COMPARE_WITH_TIMEOUT(announced.count(), 1, 2000);
        QVERIFY(waited.elapsed() >= 300);
        QCOMPARE(announced.first().first().value<TimerEngine::Phase>(), TimerEngine::Phase::Work);

        // A release that finally arrives later does not announce it a second time.
        inhibitor->completeRelease();
        QTest::qWait(50);
        QCOMPARE(announced.count(), 1);
    }

    void aConfirmedReleaseAnnouncesAtOnce()
    {
        inhibitor->deferReleases = true;
        QSignalSpy announced(notifier.get(), &PhaseNotifier::phaseEndAnnounced);
        endWorkWhileSilenced();
        QCOMPARE(announced.count(), 0);
        inhibitor->completeRelease();
        QCOMPARE(announced.count(), 1); // no need to wait for the timeout
    }

    void withNothingSilencedItIsAnnouncedStraightAway()
    {
        QSignalSpy announced(notifier.get(), &PhaseNotifier::phaseEndAnnounced);
        engine->start();
        clock.advanceSeconds(25 * 60);
        engine->refresh();
        QCOMPARE(announced.count(), 1);
    }

    void withoutAnyInhibitorSupportItIsAnnouncedStraightAway()
    {
        inhibitor->available = false; // like the no-op fallback: nothing is ever held
        QSignalSpy announced(notifier.get(), &PhaseNotifier::phaseEndAnnounced);
        engine->start();
        clock.advanceSeconds(25 * 60);
        engine->refresh();
        QCOMPARE(announced.count(), 1);
    }
};

QTEST_MAIN(PhaseNotifierTest)
#include "tst_phasenotifier.moc"
