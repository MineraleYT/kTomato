// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTest>

#include "KRunnerService.h"
#include "PresetModel.h"
#include "TimerEngine.h"

class TestKRunnerService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testActions();
    void testMatchKeyword();
    void testMatchSubCommands();
    void testMatchCustomMinutes();
    void testRunCommands();
    void testRunOpen();
    void testKeywordMustBeAWholeWord();
    void testActionButtonsRunTheirOwnCommand();
    void testMatchesOfferOnlyRelevantActions();
    void testCustomMinutesDoNotChangeThePreset();
    void testPresetByName();
};

void TestKRunnerService::testActions()
{
    TimerEngine timer;
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    const auto actions = service.Actions();
    QCOMPARE(actions.size(), 5);
    QCOMPARE(actions.at(0).id, QStringLiteral("start"));
    QCOMPARE(actions.at(1).id, QStringLiteral("pause"));
    QCOMPARE(actions.at(2).id, QStringLiteral("resume"));
    QCOMPARE(actions.at(3).id, QStringLiteral("skip"));
    QCOMPARE(actions.at(4).id, QStringLiteral("stop"));
}

void TestKRunnerService::testMatchKeyword()
{
    TimerEngine timer;
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    // Unrelated query returns nothing
    QVERIFY(service.Match(QStringLiteral("firefox")).isEmpty());
    QVERIFY(service.Match(QStringLiteral("")).isEmpty());

    // Pomodoro query returns matches
    const auto matches = service.Match(QStringLiteral("pomodoro"));
    QVERIFY(!matches.isEmpty());
    bool hasStart = false;
    bool hasOpen = false;
    for (const auto &m : matches) {
        if (m.id == QStringLiteral("start")) hasStart = true;
        if (m.id == QStringLiteral("open")) hasOpen = true;
    }
    QVERIFY(hasStart);
    QVERIFY(hasOpen);
}

void TestKRunnerService::testMatchSubCommands()
{
    TimerEngine timer;
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    auto matches = service.Match(QStringLiteral("pomodoro start"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("start"));
    QCOMPARE(matches.at(0).categoryRelevance, 100); // CategoryRelevance::Highest

    matches = service.Match(QStringLiteral("ktomato pause"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("pause"));

    matches = service.Match(QStringLiteral("pomodoro stop"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("stop"));

    matches = service.Match(QStringLiteral("pomodoro skip"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("skip"));
}

void TestKRunnerService::testMatchCustomMinutes()
{
    TimerEngine timer;
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    const auto matches = service.Match(QStringLiteral("pomodoro 25"));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("custom:25"));
    QCOMPARE(matches.at(0).categoryRelevance, 100);
}

void TestKRunnerService::testRunCommands()
{
    TimerEngine timer;
    timer.setWorkSeconds(1500);
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    QVERIFY(!timer.isRunning());

    // Start
    service.Run(QStringLiteral("start"), QString());
    QVERIFY(timer.isRunning());

    // Pause
    service.Run(QStringLiteral("pause"), QString());
    QVERIFY(timer.isPaused());

    // Resume
    service.Run(QStringLiteral("resume"), QString());
    QVERIFY(timer.isRunning());
    QVERIFY(!timer.isPaused());

    // Stop
    service.Run(QStringLiteral("stop"), QString());
    QVERIFY(!timer.isRunning());
}

void TestKRunnerService::testRunOpen()
{
    TimerEngine timer;
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    QSignalSpy spy(&service, &KRunnerService::openRequested);
    service.Run(QStringLiteral("open"), QString());
    QCOMPARE(spy.count(), 1);
}

void TestKRunnerService::testKeywordMustBeAWholeWord()
{
    TimerEngine timer;
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    QVERIFY(service.Match(QStringLiteral("tomatoes")).isEmpty());
    QVERIFY(service.Match(QStringLiteral("timers")).isEmpty());
    QVERIFY(service.Match(QStringLiteral("pomodorostart")).isEmpty());
    QVERIFY(!service.Match(QStringLiteral("Tomato")).isEmpty());
    QCOMPARE(service.Match(QStringLiteral("timer   stop")).value(0).id, QStringLiteral("stop"));
}

void TestKRunnerService::testActionButtonsRunTheirOwnCommand()
{
    TimerEngine timer;
    timer.setWorkSeconds(1500);
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    service.Run(QStringLiteral("start"), QString());
    QVERIFY(timer.isRunning());

    // "Pause" pressed on the "Stop timer" result pauses; it does not stop.
    service.Run(QStringLiteral("stop"), QStringLiteral("pause"));
    QVERIFY(timer.isPaused());

    service.Run(QStringLiteral("resume"), QStringLiteral("stop"));
    QVERIFY(!timer.isActive());
}

void TestKRunnerService::testMatchesOfferOnlyRelevantActions()
{
    TimerEngine timer;
    timer.setWorkSeconds(1500);
    PresetModel presets;
    KRunnerService service(&timer, &presets);
    const QString actionsKey = QStringLiteral("actions");

    // Command results carry no buttons at all (an explicit empty list).
    auto matches = service.Match(QStringLiteral("pomodoro stop"));
    QVERIFY(matches.at(0).properties.contains(actionsKey));
    QVERIFY(matches.at(0).properties.value(actionsKey).toStringList().isEmpty());

    timer.start();
    matches = service.Match(QStringLiteral("pomodoro"));
    QCOMPARE(matches.at(0).id, QStringLiteral("pause"));
    QCOMPARE(matches.at(0).properties.value(actionsKey).toStringList(),
             (QStringList{QStringLiteral("pause"), QStringLiteral("skip"), QStringLiteral("stop")}));
    for (const auto &m : matches) {
        QVERIFY2(m.properties.contains(actionsKey), qPrintable(m.id));
    }

    timer.pause();
    matches = service.Match(QStringLiteral("pomodoro"));
    QCOMPARE(matches.at(0).id, QStringLiteral("resume"));
    QCOMPARE(matches.at(0).properties.value(actionsKey).toStringList(),
             (QStringList{QStringLiteral("resume"), QStringLiteral("skip"), QStringLiteral("stop")}));
}

void TestKRunnerService::testCustomMinutesDoNotChangeThePreset()
{
    TimerEngine timer;
    timer.setWorkSeconds(1500);
    PresetModel presets;
    KRunnerService service(&timer, &presets);

    service.Run(QStringLiteral("custom:45"), QString());
    QVERIFY(timer.isRunning());
    QVERIFY(timer.remainingSeconds() > 44 * 60);
    QCOMPARE(timer.workSeconds(), 1500);

    QVERIFY(service.Match(QStringLiteral("pomodoro 0")).isEmpty());
    QVERIFY(service.Match(QStringLiteral("pomodoro 999")).isEmpty());
}

void TestKRunnerService::testPresetByName()
{
    TimerEngine timer;
    PresetModel presets;
    presets.addStandardPresets();
    KRunnerService service(&timer, &presets);
    QVERIFY(presets.count() > 1);

    // Pick a preset other than the current one and search for part of its name.
    const QModelIndex target = presets.index(presets.count() - 1, 0);
    const QString uuid = target.data(PresetModel::UuidRole).toString();
    const QString name = target.data(PresetModel::NameRole).toString();
    QVERIFY(uuid != presets.currentUuid());

    const auto matches = service.Match(QStringLiteral("pomodoro ") + name.left(4).toUpper());
    bool found = false;
    for (const auto &m : matches) {
        QVERIFY(m.id.startsWith(QStringLiteral("preset:")));
        found = found || m.id == QStringLiteral("preset:") + uuid;
    }
    QVERIFY(found);

    QVERIFY(service.Match(QStringLiteral("pomodoro no-such-preset-name")).isEmpty());

    service.Run(QStringLiteral("preset:") + uuid, QString());
    QCOMPARE(presets.currentUuid(), uuid);
    QVERIFY(timer.isRunning());
}

QTEST_MAIN(TestKRunnerService)
#include "tst_krunnerservice.moc"
