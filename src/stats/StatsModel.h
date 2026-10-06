// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QDate>
#include <QHash>
#include <QList>
#include <QLocale>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

#include "SessionRepository.h"
#include "StatsAggregator.h"
#include "StatsTypes.h"

class SessionRecorder;

/**
 * Presentation model for the statistics page.
 *
 * Provides aggregated session data across Day, Week, Month, and Year periods,
 * grouped bar chart data (work vs. breaks), category/timer breakdowns,
 * streak tracking, and CSV export.
 *
 * By default it follows the system: the time zone, the default QLocale (which follows the
 * application language) and the first day of the week are re-read by refreshEnvironment()
 * and when the date changes. "Today" is kept current by a timer armed for the next local
 * midnight, backed by a coarse periodic check that also catches suspend and clock changes.
 * Tests inject a fixed Environment with setEnvironment().
 */
class StatsModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class Period {
        Day,
        Week,
        Month,
        Year,
    };
    Q_ENUM(Period)

    enum class BreakdownMode {
        ByTimer,
        ByCategory,
        ByTask,
    };
    Q_ENUM(BreakdownMode)

    struct Environment {
        std::function<QDateTime()> now = []() { return QDateTime::currentDateTime(); };
        QTimeZone timeZone = QTimeZone::systemTimeZone();
        QLocale locale = QLocale();
        Qt::DayOfWeek firstDayOfWeek = QLocale().firstDayOfWeek();
        /// If true, refreshEnvironment() and date changes re-read the time zone, locale and first
        /// day of the week from the system (keeping `now`). setEnvironment() honours it.
        bool followSystem = false;
    };

    Q_PROPERTY(Period period READ period WRITE setPeriod NOTIFY periodChanged FINAL)
    Q_PROPERTY(BreakdownMode breakdownMode READ breakdownMode WRITE setBreakdownMode NOTIFY breakdownModeChanged FINAL)
    Q_PROPERTY(bool includeInterrupted READ includeInterrupted WRITE setIncludeInterrupted NOTIFY includeInterruptedChanged FINAL)

    Q_PROPERTY(QDate anchorDate READ anchorDate NOTIFY periodChanged FINAL)
    Q_PROPERTY(QString title READ title NOTIFY periodChanged FINAL)
    Q_PROPERTY(bool isCurrent READ isCurrent NOTIFY periodChanged FINAL)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY periodChanged FINAL)

    Q_PROPERTY(qint64 workSeconds READ workSeconds NOTIFY statsChanged FINAL)
    Q_PROPERTY(qint64 breakSeconds READ breakSeconds NOTIFY statsChanged FINAL)
    Q_PROPERTY(int completedWorkSessions READ completedWorkSessions NOTIFY statsChanged FINAL)
    Q_PROPERTY(int interruptedWorkSessions READ interruptedWorkSessions NOTIFY statsChanged FINAL)
    Q_PROPERTY(double ratio READ ratio NOTIFY statsChanged FINAL)
    Q_PROPERTY(bool hasData READ hasData NOTIFY statsChanged FINAL)

    Q_PROPERTY(int currentStreak READ currentStreak NOTIFY streaksChanged FINAL)
    Q_PROPERTY(int bestStreak READ bestStreak NOTIFY streaksChanged FINAL)

    Q_PROPERTY(int todayCompletedCount READ todayCompletedCount NOTIFY statsChanged FINAL)
    Q_PROPERTY(int dailyGoal READ dailyGoal WRITE setDailyGoal NOTIFY dailyGoalChanged FINAL)
    Q_PROPERTY(double dailyGoalProgress READ dailyGoalProgress NOTIFY statsChanged FINAL)
    /// False while the goal is 0 (disabled).
    Q_PROPERTY(bool dailyGoalReached READ dailyGoalReached NOTIFY statsChanged FINAL)
    Q_PROPERTY(bool protectWeekendStreak READ protectWeekendStreak WRITE setProtectWeekendStreak NOTIFY protectWeekendStreakChanged FINAL)

    Q_PROPERTY(QVariantList buckets READ buckets NOTIFY statsChanged FINAL)
    Q_PROPERTY(QVariantList breakdown READ breakdown NOTIFY statsChanged FINAL)
    Q_PROPERTY(qint64 maxBucketSeconds READ maxBucketSeconds NOTIFY statsChanged FINAL)

    explicit StatsModel(QObject *parent = nullptr);

    void attach(SessionRepository *repository, SessionRecorder *recorder = nullptr);
    void setEnvironment(const Environment &env);

    /// Re-checks the current date: on a new day it updates "today", the goal count and the
    /// streaks, and moves the shown period along if it was showing the old today. Called by the
    /// timers; public for tests.
    void checkDateChange();

    Period period() const { return m_period; }
    void setPeriod(Period period);

    BreakdownMode breakdownMode() const { return m_breakdownMode; }
    void setBreakdownMode(BreakdownMode mode);

    bool includeInterrupted() const { return m_includeInterrupted; }
    void setIncludeInterrupted(bool include);

    QDate anchorDate() const { return m_anchorDate; }
    QString title() const;
    bool isCurrent() const;
    bool canGoNext() const;

    qint64 workSeconds() const { return m_summary.workSeconds; }
    qint64 breakSeconds() const { return m_summary.breakSeconds; }
    int completedWorkSessions() const { return m_summary.completedWorkSessions; }
    int interruptedWorkSessions() const { return m_summary.interruptedWorkSessions; }
    double ratio() const { return m_summary.ratio; }
    bool hasData() const { return m_summary.hasData(); }

    int currentStreak() const { return m_currentStreak; }
    int bestStreak() const { return m_bestStreak; }

    int todayCompletedCount() const { return m_todayCompletedCount; }
    int dailyGoal() const { return m_dailyGoal; }
    /// 0 disables the goal; negative values are treated as 0.
    void setDailyGoal(int goal);
    double dailyGoalProgress() const;
    bool dailyGoalReached() const { return m_dailyGoal > 0 && m_todayCompletedCount >= m_dailyGoal; }

    bool protectWeekendStreak() const { return m_protectWeekendStreak; }
    void setProtectWeekendStreak(bool protect);

    QVariantList buckets() const { return m_bucketsVariant; }
    QVariantList breakdown() const;
    qint64 maxBucketSeconds() const { return m_maxBucketSeconds; }

    Q_INVOKABLE void previousPeriod();
    Q_INVOKABLE void nextPeriod();
    Q_INVOKABLE void today();
    Q_INVOKABLE void refresh();
    /// Re-reads the system time zone and the default QLocale (e.g. after the application
    /// language changed), then recomputes labels, titles, "today", streaks and the period.
    Q_INVOKABLE void refreshEnvironment();
    /// Writes the sessions of the shown period (honouring includeInterrupted) to a local file.
    /// On failure emits exportFailed() and returns false; lastExportError() keeps the message.
    Q_INVOKABLE bool exportCsv(const QUrl &fileUrl);
    Q_INVOKABLE QString lastExportError() const { return m_lastExportError; }
    Q_INVOKABLE QString formatDuration(qint64 seconds) const;
    Q_INVOKABLE QString formatRatio(double ratio) const;

Q_SIGNALS:
    void periodChanged();
    void breakdownModeChanged();
    void includeInterruptedChanged();
    void statsChanged();
    void streaksChanged();
    void dailyGoalChanged();
    void protectWeekendStreakChanged();
    /// A translated, user-presentable reason why exportCsv() failed.
    void exportFailed(const QString &message);

private:
    void recompute();
    void recomputeStreaks();
    void reloadCompletedStarts();
    void loadNewCompletedStarts();
    void rebuildCompletedDays();
    void onSessionRecorded();
    void readSystemEnvironment();
    void armMidnightTimer();
    QDate todayDate() const;
    StatsPeriod toStatsPeriod(Period p) const;

    SessionRepository *m_repository = nullptr;
    Environment m_env;

    Period m_period = Period::Week;
    BreakdownMode m_breakdownMode = BreakdownMode::ByTimer;
    bool m_includeInterrupted = true;
    QDate m_anchorDate;

    StatsSummary m_summary;
    QList<SessionRecord> m_currentRecords;
    QVariantList m_bucketsVariant;
    qint64 m_maxBucketSeconds = 3600;

    int m_currentStreak = 0;
    int m_bestStreak = 0;
    int m_todayCompletedCount = 0;
    int m_dailyGoal = 8;
    bool m_protectWeekendStreak = true;

    QDate m_today;                       ///< "Today" as of the last check, in m_env.timeZone.
    QTimer m_midnightTimer;              ///< Single shot, armed for the next local midnight.
    QTimer m_dateCheckTimer;             ///< Coarse periodic check (suspend, clock changes).
    QList<qint64> m_completedStarts;     ///< Cache: start instants of completed work sessions.
    qint64 m_completedStartsMaxId = 0;   ///< Highest session id in the cache.
    QHash<QDate, int> m_completedPerDay; ///< Cache: completed work sessions per local day.
    QString m_lastExportError;
};
