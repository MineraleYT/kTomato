// SPDX-License-Identifier: GPL-3.0-or-later
#include <KLocalizedString>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "Database.h"
#include "PresetModel.h"
#include "PresetPersistence.h"
#include "PresetRepository.h"
#include "SessionRecorder.h"
#include "SessionRepository.h"
#include "StatsModel.h"
#include "TimerEngine.h"

class TestStatsModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("ktomato");
    }

    void init();
    void cleanup();

    void testNavigation();
    void testReactiveUpdateOnRecord();
    void testFormatters();
    void testExportCsv();
    void testBreakdownModes();
    void testDateRolloverMovesTodayAlong();
    void testDateRolloverKeepsAPastPeriod();
    void testRolloverIsNoticedAfterAnEarlierRefresh();
    void testRefreshEnvironmentFollowsDefaultLocale();
    void testWeekTitles();
    void testDailyGoalZeroIsDisabled();
    void testTodayCountAndStreakUpdateOnRecord();
    void testExportRejectsRemoteUrls();
    void testExportHonoursIncludeInterrupted();
    void testExportReportsWriteFailure();

private:
    void insertWork(const QDateTime &start, bool completed = true);

    std::unique_ptr<Database> m_db;
    std::unique_ptr<SessionRepository> m_repo;
    std::unique_ptr<StatsModel> m_model;
    StatsModel::Environment m_env;
};

void TestStatsModel::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"));
    QVERIFY(m_db->open());
    m_repo = std::make_unique<SessionRepository>(m_db->connectionName());

    m_env.timeZone = QTimeZone::utc();
    m_env.firstDayOfWeek = Qt::Monday;
    m_env.now = []() {
        return QDateTime(QDate(2026, 10, 3), QTime(12, 0), QTimeZone::utc());
    };

    m_model = std::make_unique<StatsModel>();
    m_model->setEnvironment(m_env);
    m_model->attach(m_repo.get());
}

void TestStatsModel::cleanup()
{
    m_model.reset();
    m_repo.reset();
    m_db.reset();
}

void TestStatsModel::testNavigation()
{
    m_model->setPeriod(StatsModel::Period::Day);
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 3));
    QVERIFY(m_model->isCurrent());
    QVERIFY(!m_model->canGoNext()); // Cannot go into future

    m_model->previousPeriod();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 2));
    QVERIFY(!m_model->isCurrent());
    QVERIFY(m_model->canGoNext());

    m_model->nextPeriod();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 3));
    QVERIFY(m_model->isCurrent());

    m_model->previousPeriod();
    m_model->previousPeriod();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 1));
    m_model->today();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 3));
}

void TestStatsModel::testReactiveUpdateOnRecord()
{
    PresetRepository presetRepo(m_db->connectionName());
    PresetModel presets;
    PresetPersistence persistence(&presets, &presetRepo);
    TimerEngine engine;
    SessionRecorder recorder(&engine, &presets, m_repo.get());

    m_model->attach(m_repo.get(), &recorder);
    QCOMPARE(m_model->workSeconds(), 0);

    QSignalSpy spy(m_model.get(), &StatsModel::statsChanged);

    // Insert a session into repository and fire signal
    SessionRecord r;
    r.kind = SessionKind::Work;
    r.startedAtMs = m_env.now().toMSecsSinceEpoch() - 1500000;
    r.endedAtMs = m_env.now().toMSecsSinceEpoch();
    r.durationSec = 1500;
    r.plannedSec = 1500;
    r.completed = true;
    r.presetName = QStringLiteral("Default");
    m_repo->insert(r);

    Q_EMIT recorder.sessionRecorded();

    QVERIFY(spy.count() > 0);
    QCOMPARE(m_model->workSeconds(), 1500);
    QCOMPARE(m_model->completedWorkSessions(), 1);
    QVERIFY(m_model->hasData());
}

void TestStatsModel::testFormatters()
{
    QCOMPARE(m_model->formatDuration(0), QStringLiteral("0m"));
    QCOMPARE(m_model->formatDuration(45), QStringLiteral("45s"));
    QCOMPARE(m_model->formatDuration(300), QStringLiteral("5m"));
    QCOMPARE(m_model->formatDuration(3600), QStringLiteral("1h"));
    QCOMPARE(m_model->formatDuration(5400), QStringLiteral("1h 30m"));

    QCOMPARE(m_model->formatRatio(-1.0), QStringLiteral("—"));
    QCOMPARE(m_model->formatRatio(5.0), QStringLiteral("5.0 : 1"));
}

void TestStatsModel::testExportCsv()
{
    SessionRecord r;
    r.kind = SessionKind::Work;
    r.startedAtMs = m_env.now().toMSecsSinceEpoch() - 1500000;
    r.endedAtMs = m_env.now().toMSecsSinceEpoch();
    r.durationSec = 1500;
    r.plannedSec = 1500;
    r.completed = true;
    m_repo->insert(r);
    m_model->refresh();

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString filePath = tempDir.filePath(QStringLiteral("test_stats.csv"));

    QVERIFY(m_model->exportCsv(QUrl::fromLocalFile(filePath)));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("start,end,phase")));
    QVERIFY(content.contains(QStringLiteral("work,1500,1500,yes")));
}

void TestStatsModel::testBreakdownModes()
{
    SessionRecord r1;
    r1.kind = SessionKind::Work;
    r1.startedAtMs = m_env.now().toMSecsSinceEpoch() - 3600000;
    r1.endedAtMs = r1.startedAtMs + 2000000;
    r1.durationSec = 2000;
    r1.plannedSec = 2000;
    r1.completed = true;
    r1.presetName = QStringLiteral("Timer A");
    r1.category = QStringLiteral("Coding");
    r1.note = QStringLiteral("Refactor auth");
    m_repo->insert(r1);

    SessionRecord r2;
    r2.kind = SessionKind::Work;
    r2.startedAtMs = m_env.now().toMSecsSinceEpoch() - 1000000;
    r2.endedAtMs = m_env.now().toMSecsSinceEpoch();
    r2.durationSec = 1000;
    r2.plannedSec = 1000;
    r2.completed = true;
    r2.presetName = QStringLiteral("Timer B");
    r2.category = QStringLiteral("");
    r2.note = QStringLiteral("");
    m_repo->insert(r2);

    m_model->refresh();

    // ByTimer
    m_model->setBreakdownMode(StatsModel::BreakdownMode::ByTimer);
    QVariantList list = m_model->breakdown();
    QCOMPARE(list.size(), 2);

    // ByCategory
    m_model->setBreakdownMode(StatsModel::BreakdownMode::ByCategory);
    list = m_model->breakdown();
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Coding"));
    QCOMPARE(list.at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Uncategorized"));

    // ByTask
    m_model->setBreakdownMode(StatsModel::BreakdownMode::ByTask);
    list = m_model->breakdown();
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Refactor auth"));
    QCOMPARE(list.at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Without note"));
}

void TestStatsModel::insertWork(const QDateTime &start, bool completed)
{
    SessionRecord r;
    r.kind = SessionKind::Work;
    r.startedAtMs = start.toMSecsSinceEpoch();
    r.endedAtMs = r.startedAtMs + 1'500'000;
    r.durationSec = completed ? 1500 : 600;
    r.plannedSec = 1500;
    r.completed = completed;
    r.presetName = QStringLiteral("Default");
    QVERIFY(m_repo->insert(r));
}

void TestStatsModel::testDateRolloverMovesTodayAlong()
{
    auto now = std::make_shared<QDateTime>(QDate(2026, 10, 3), QTime(23, 59), QTimeZone::utc());
    StatsModel::Environment env = m_env;
    env.now = [now]() { return *now; };
    m_model->setEnvironment(env);
    m_model->setPeriod(StatsModel::Period::Day);

    insertWork(QDateTime(QDate(2026, 10, 3), QTime(10, 0), QTimeZone::utc()));
    m_model->refresh();
    QCOMPARE(m_model->todayCompletedCount(), 1);
    QCOMPARE(m_model->currentStreak(), 1);
    QCOMPARE(m_model->completedWorkSessions(), 1);

    QSignalSpy periodSpy(m_model.get(), &StatsModel::periodChanged);
    m_model->checkDateChange(); // same day: nothing happens
    QCOMPARE(periodSpy.count(), 0);

    *now = QDateTime(QDate(2026, 10, 4), QTime(0, 0, 30), QTimeZone::utc());
    m_model->checkDateChange();
    QCOMPARE(periodSpy.count(), 1);
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 4));
    QVERIFY(m_model->isCurrent());
    QCOMPARE(m_model->todayCompletedCount(), 0);
    QCOMPARE(m_model->currentStreak(), 1); // the new day has not broken the streak yet
    QCOMPARE(m_model->completedWorkSessions(), 0);
}

void TestStatsModel::testDateRolloverKeepsAPastPeriod()
{
    auto now = std::make_shared<QDateTime>(QDate(2026, 10, 3), QTime(23, 59), QTimeZone::utc());
    StatsModel::Environment env = m_env;
    env.now = [now]() { return *now; };
    m_model->setEnvironment(env);
    m_model->setPeriod(StatsModel::Period::Day);
    m_model->previousPeriod();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 2));

    *now = QDateTime(QDate(2026, 10, 4), QTime(0, 1), QTimeZone::utc());
    m_model->checkDateChange();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 2)); // the user looked back on purpose

    // The week view of the old today still contains the new today: it follows along.
    m_model->setPeriod(StatsModel::Period::Week);
    m_model->today();
    *now = QDateTime(QDate(2026, 10, 5), QTime(0, 1), QTimeZone::utc());
    m_model->checkDateChange();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 5));
    QVERIFY(m_model->isCurrent());
}

void TestStatsModel::testRolloverIsNoticedAfterAnEarlierRefresh()
{
    // After midnight, something (a recorded session, a note) recomputes the streaks before the
    // midnight timer fires. That must not make the model believe it is still the old day.
    auto now = std::make_shared<QDateTime>(QDate(2026, 10, 3), QTime(23, 59), QTimeZone::utc());
    StatsModel::Environment env = m_env;
    env.now = [now]() { return *now; };
    m_model->setEnvironment(env);
    m_model->setPeriod(StatsModel::Period::Day);
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 3));

    *now = QDateTime(QDate(2026, 10, 4), QTime(0, 0, 30), QTimeZone::utc());
    m_model->refresh();
    QCOMPARE(m_model->todayCompletedCount(), 0); // already counts the new day
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 3));

    m_model->checkDateChange();
    QCOMPARE(m_model->anchorDate(), QDate(2026, 10, 4));
    QVERIFY(m_model->isCurrent());
}

void TestStatsModel::testRefreshEnvironmentFollowsDefaultLocale()
{
    const QLocale original;
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    StatsModel::Environment env = m_env;
    env.followSystem = true;
    m_model->setEnvironment(env);
    m_model->setPeriod(StatsModel::Period::Month);
    QCOMPARE(m_model->title(), QStringLiteral("Oktober 2026"));

    QSignalSpy periodSpy(m_model.get(), &StatsModel::periodChanged);
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    m_model->refreshEnvironment();
    QCOMPARE(m_model->title(), QStringLiteral("October 2026"));
    QVERIFY(periodSpy.count() > 0);

    m_model->setPeriod(StatsModel::Period::Year);
    m_model->setPeriod(StatsModel::Period::Week);
    // en_US weeks start on Sunday.
    const QVariantList buckets = m_model->buckets();
    QCOMPARE(buckets.size(), 7);
    QCOMPARE(buckets.first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Sun"));

    QLocale::setDefault(original);
}

void TestStatsModel::testWeekTitles()
{
    auto now = std::make_shared<QDateTime>(QDate(2026, 10, 3), QTime(12, 0), QTimeZone::utc());
    StatsModel::Environment env = m_env;
    env.locale = QLocale(QLocale::English, QLocale::UnitedStates); // first day of week stays Monday (m_env)
    env.now = [now]() { return *now; };
    m_model->setEnvironment(env);
    m_model->setPeriod(StatsModel::Period::Week);
    QCOMPARE(m_model->title(), QStringLiteral("28 Sep – 4 Oct 2026"));
    m_model->nextPeriod(); // not allowed into the future
    m_model->previousPeriod();
    QCOMPARE(m_model->title(), QStringLiteral("21 – 27 September 2026"));

    *now = QDateTime(QDate(2027, 1, 1), QTime(12, 0), QTimeZone::utc());
    m_model->setEnvironment(env);
    QCOMPARE(m_model->title(), QStringLiteral("28 Dec 2026 – 3 Jan 2027"));
}

void TestStatsModel::testDailyGoalZeroIsDisabled()
{
    insertWork(QDateTime(QDate(2026, 10, 3), QTime(9, 0), QTimeZone::utc()));
    m_model->refresh();
    m_model->setDailyGoal(1);
    QVERIFY(m_model->dailyGoalReached());
    QCOMPARE(m_model->dailyGoalProgress(), 1.0);

    m_model->setDailyGoal(0);
    QCOMPARE(m_model->dailyGoal(), 0);
    QVERIFY(!m_model->dailyGoalReached());
    QCOMPARE(m_model->dailyGoalProgress(), 0.0);

    m_model->setDailyGoal(-3);
    QCOMPARE(m_model->dailyGoal(), 0);
}

void TestStatsModel::testTodayCountAndStreakUpdateOnRecord()
{
    PresetRepository presetRepo(m_db->connectionName());
    PresetModel presets;
    PresetPersistence persistence(&presets, &presetRepo);
    TimerEngine engine;
    SessionRecorder recorder(&engine, &presets, m_repo.get());
    m_model->attach(m_repo.get(), &recorder);

    insertWork(QDateTime(QDate(2026, 10, 2), QTime(9, 0), QTimeZone::utc()));
    m_model->refresh();
    QCOMPARE(m_model->todayCompletedCount(), 0);
    QCOMPARE(m_model->currentStreak(), 1);

    // Recorded sessions extend the cache incrementally.
    insertWork(QDateTime(QDate(2026, 10, 3), QTime(9, 0), QTimeZone::utc()));
    insertWork(QDateTime(QDate(2026, 10, 3), QTime(10, 0), QTimeZone::utc()), false);
    Q_EMIT recorder.sessionRecorded();
    QCOMPARE(m_model->todayCompletedCount(), 1);
    QCOMPARE(m_model->currentStreak(), 2);
    QCOMPARE(m_model->bestStreak(), 2);

    // A full refresh (after clear/restore) starts over.
    QVERIFY(m_repo->deleteAll());
    m_model->refresh();
    QCOMPARE(m_model->todayCompletedCount(), 0);
    QCOMPARE(m_model->currentStreak(), 0);
}

void TestStatsModel::testExportRejectsRemoteUrls()
{
    QSignalSpy failed(m_model.get(), &StatsModel::exportFailed);
    QVERIFY(!m_model->exportCsv(QUrl(QStringLiteral("sftp://example.org/stats.csv"))));
    QCOMPARE(failed.count(), 1);
    QVERIFY(!failed.first().first().toString().isEmpty());
    QCOMPARE(m_model->lastExportError(), failed.first().first().toString());
}

void TestStatsModel::testExportHonoursIncludeInterrupted()
{
    insertWork(QDateTime(QDate(2026, 10, 3), QTime(9, 0), QTimeZone::utc()));
    insertWork(QDateTime(QDate(2026, 10, 3), QTime(10, 0), QTimeZone::utc()), false);
    m_model->refresh();

    QTemporaryDir tempDir;
    const QString filePath = tempDir.filePath(QStringLiteral("stats.csv"));
    auto exportedRows = [&]() {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return -1;
        }
        return int(QString::fromUtf8(file.readAll()).split(QStringLiteral("\r\n"), Qt::SkipEmptyParts).size()) - 1;
    };

    QVERIFY(m_model->exportCsv(QUrl::fromLocalFile(filePath)));
    QCOMPARE(exportedRows(), 2);

    m_model->setIncludeInterrupted(false);
    QVERIFY(m_model->exportCsv(QUrl::fromLocalFile(filePath)));
    QCOMPARE(exportedRows(), 1);
}

void TestStatsModel::testExportReportsWriteFailure()
{
    QTemporaryDir tempDir;
    QSignalSpy failed(m_model.get(), &StatsModel::exportFailed);
    const QString filePath = tempDir.filePath(QStringLiteral("missing-dir/stats.csv"));
    QVERIFY(!m_model->exportCsv(QUrl::fromLocalFile(filePath)));
    QCOMPARE(failed.count(), 1);
    QVERIFY(!QFile::exists(filePath));
}

QTEST_MAIN(TestStatsModel)
#include "tst_statsmodel.moc"
