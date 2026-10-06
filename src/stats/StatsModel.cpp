// SPDX-License-Identifier: GPL-3.0-or-later
#include "StatsModel.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>

#include <KLocalizedString>

#include "CsvExporter.h"
#include "SessionRecorder.h"

Q_LOGGING_CATEGORY(lcStatsModel, "ktomato.stats.model")

namespace
{
StatsPeriod toAggregatorPeriod(StatsModel::Period p)
{
    switch (p) {
    case StatsModel::Period::Day:
        return StatsPeriod::Day;
    case StatsModel::Period::Week:
        return StatsPeriod::Week;
    case StatsModel::Period::Month:
        return StatsPeriod::Month;
    case StatsModel::Period::Year:
        return StatsPeriod::Year;
    }
    Q_UNREACHABLE_RETURN(StatsPeriod::Week);
}

/// The periodic safety net behind the midnight timer: a suspended machine or a changed clock
/// makes a single-shot timer fire late (or never), this catches up within a few minutes.
constexpr int kDateCheckIntervalMs = 5 * 60 * 1000;

QString capitalized(QString s)
{
    if (!s.isEmpty()) {
        s[0] = s[0].toUpper();
    }
    return s;
}
} // namespace

StatsModel::StatsModel(QObject *parent)
    : QObject(parent)
{
    m_env.followSystem = true;
    readSystemEnvironment();
    m_today = todayDate();
    m_anchorDate = m_today;

    m_midnightTimer.setSingleShot(true);
    m_midnightTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_midnightTimer, &QTimer::timeout, this, &StatsModel::checkDateChange);
    m_dateCheckTimer.setInterval(kDateCheckIntervalMs);
    m_dateCheckTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_dateCheckTimer, &QTimer::timeout, this, &StatsModel::checkDateChange);
    m_dateCheckTimer.start();
    armMidnightTimer();
}

void StatsModel::attach(SessionRepository *repository, SessionRecorder *recorder)
{
    m_repository = repository;
    if (recorder) {
        connect(recorder, &SessionRecorder::sessionRecorded, this, &StatsModel::onSessionRecorded, Qt::UniqueConnection);
    }
    refresh();
}

void StatsModel::setEnvironment(const Environment &env)
{
    m_env = env;
    if (m_env.followSystem) {
        readSystemEnvironment();
    }
    m_today = todayDate();
    m_anchorDate = m_today;
    armMidnightTimer();
    refresh();
    Q_EMIT periodChanged();
}

QDate StatsModel::todayDate() const
{
    return m_env.now().toTimeZone(m_env.timeZone).date();
}

void StatsModel::readSystemEnvironment()
{
    m_env.timeZone = QTimeZone::systemTimeZone();
    m_env.locale = QLocale();
    m_env.firstDayOfWeek = m_env.locale.firstDayOfWeek();
}

void StatsModel::armMidnightTimer()
{
    const QDateTime now = m_env.now().toTimeZone(m_env.timeZone);
    const QDateTime nextMidnight = now.date().addDays(1).startOfDay(m_env.timeZone);
    // A little past midnight, so the new day has surely begun; never a busy loop, and never
    // longer than a day (the periodic check covers anything this misses).
    const qint64 ms = now.msecsTo(nextMidnight) + 1000;
    m_midnightTimer.start(int(qBound<qint64>(1000, ms, 24 * 3600 * 1000LL + 1000)));
}

void StatsModel::checkDateChange()
{
    const QTimeZone oldZone = m_env.timeZone;
    if (m_env.followSystem) {
        m_env.timeZone = QTimeZone::systemTimeZone();
    }
    const bool zoneChanged = m_env.timeZone != oldZone;
    const QDate newToday = todayDate();
    armMidnightTimer();
    if (newToday == m_today && !zoneChanged) {
        return;
    }

    const QDate oldToday = m_today;
    m_today = newToday;
    // Follow along if the shown period was the one containing the old today.
    const StatsPeriod sp = toStatsPeriod(m_period);
    const PeriodRange shown = Stats::rangeFor(sp, m_anchorDate, m_env.firstDayOfWeek, m_env.timeZone);
    if (oldToday >= shown.firstDay && oldToday <= shown.lastDay) {
        m_anchorDate = newToday;
    }
    if (zoneChanged) {
        rebuildCompletedDays();
    }
    recompute();
    recomputeStreaks();
    Q_EMIT periodChanged();
}

void StatsModel::refreshEnvironment()
{
    if (m_env.followSystem) {
        readSystemEnvironment();
    }
    const QDate oldToday = m_today;
    m_today = todayDate();
    if (m_anchorDate == oldToday) {
        m_anchorDate = m_today;
    }
    armMidnightTimer();
    rebuildCompletedDays();
    recompute();
    recomputeStreaks();
    Q_EMIT periodChanged();
}

StatsPeriod StatsModel::toStatsPeriod(Period p) const
{
    return toAggregatorPeriod(p);
}

void StatsModel::setPeriod(Period period)
{
    if (m_period == period) {
        return;
    }
    m_period = period;
    recompute();
    Q_EMIT periodChanged();
}

void StatsModel::setBreakdownMode(BreakdownMode mode)
{
    if (m_breakdownMode == mode) {
        return;
    }
    m_breakdownMode = mode;
    Q_EMIT breakdownModeChanged();
    Q_EMIT statsChanged();
}

void StatsModel::setIncludeInterrupted(bool include)
{
    if (m_includeInterrupted == include) {
        return;
    }
    m_includeInterrupted = include;
    recompute();
    Q_EMIT includeInterruptedChanged();
}

QString StatsModel::title() const
{
    const QLocale &locale = m_env.locale;
    const StatsPeriod sp = toStatsPeriod(m_period);
    const PeriodRange range = Stats::rangeFor(sp, m_anchorDate, m_env.firstDayOfWeek, m_env.timeZone);

    switch (m_period) {
    case Period::Day:
        return capitalized(locale.toString(m_anchorDate, QLocale::LongFormat));

    case Period::Week: {
        // The formats are translatable so each language can order day, month and year its own way.
        QString first;
        QString last;
        if (range.firstDay.year() == range.lastDay.year()) {
            if (range.firstDay.month() == range.lastDay.month()) {
                first = locale.toString(range.firstDay,
                                        i18nc("QDate::toString() format of the first day of a week within one month, "
                                              "e.g. \"5\" in \"5 – 11 October 2026\"",
                                              "d"));
                last = locale.toString(range.lastDay,
                                       i18nc("QDate::toString() format of the last day of a week within one month, "
                                             "e.g. \"11 October 2026\" in \"5 – 11 October 2026\"",
                                             "d MMMM yyyy"));
            } else {
                first = locale.toString(range.firstDay,
                                        i18nc("QDate::toString() format of the first day of a week spanning two months, "
                                              "e.g. \"28 Sep\" in \"28 Sep – 4 Oct 2026\"",
                                              "d MMM"));
                last = locale.toString(range.lastDay,
                                       i18nc("QDate::toString() format of the last day of a week spanning two months, "
                                             "e.g. \"4 Oct 2026\" in \"28 Sep – 4 Oct 2026\"",
                                             "d MMM yyyy"));
            }
        } else {
            const QString format = i18nc("QDate::toString() format of either day of a week spanning two years, "
                                         "e.g. \"29 Dec 2025\"",
                                         "d MMM yyyy");
            first = locale.toString(range.firstDay, format);
            last = locale.toString(range.lastDay, format);
        }
        return capitalized(i18nc("@title week range: %1 first day, %2 last day", "%1 – %2", first, last));
    }

    case Period::Month:
        return capitalized(i18nc("@title %1 month name, %2 year", "%1 %2",
                                 locale.standaloneMonthName(m_anchorDate.month(), QLocale::LongFormat),
                                 QString::number(m_anchorDate.year())));

    case Period::Year:
        return QString::number(m_anchorDate.year());
    }

    return QString();
}

bool StatsModel::isCurrent() const
{
    const qint64 nowMs = m_env.now().toMSecsSinceEpoch();
    const PeriodRange range = Stats::rangeFor(toStatsPeriod(m_period), m_anchorDate, m_env.firstDayOfWeek, m_env.timeZone);
    return range.startMs <= nowMs && nowMs < range.endMs;
}

bool StatsModel::canGoNext() const
{
    const qint64 nowMs = m_env.now().toMSecsSinceEpoch();
    const QDate nextAnchor = Stats::shifted(toStatsPeriod(m_period), m_anchorDate, 1);
    const PeriodRange nextRange = Stats::rangeFor(toStatsPeriod(m_period), nextAnchor, m_env.firstDayOfWeek, m_env.timeZone);
    return nextRange.startMs <= nowMs;
}

void StatsModel::previousPeriod()
{
    m_anchorDate = Stats::shifted(toStatsPeriod(m_period), m_anchorDate, -1);
    recompute();
    Q_EMIT periodChanged();
}

void StatsModel::nextPeriod()
{
    if (!canGoNext()) {
        return;
    }
    m_anchorDate = Stats::shifted(toStatsPeriod(m_period), m_anchorDate, 1);
    recompute();
    Q_EMIT periodChanged();
}

void StatsModel::today()
{
    const QDate current = todayDate();
    if (m_anchorDate == current) {
        return;
    }
    m_anchorDate = current;
    recompute();
    Q_EMIT periodChanged();
}

void StatsModel::refresh()
{
    // Full reload: history may have been cleared, restored or edited.
    reloadCompletedStarts();
    recompute();
    recomputeStreaks();
}

void StatsModel::onSessionRecorded()
{
    // A new row only: extend the cache instead of reloading the whole history.
    loadNewCompletedStarts();
    recompute();
    recomputeStreaks();
}

void StatsModel::reloadCompletedStarts()
{
    m_completedStarts.clear();
    m_completedStartsMaxId = 0;
    loadNewCompletedStarts();
    rebuildCompletedDays();
}

void StatsModel::loadNewCompletedStarts()
{
    if (!m_repository) {
        return;
    }
    const QList<qint64> added = m_repository->completedWorkStartsAfter(m_completedStartsMaxId, &m_completedStartsMaxId);
    m_completedStarts.append(added);
    for (const qint64 ms : added) {
        ++m_completedPerDay[QDateTime::fromMSecsSinceEpoch(ms, m_env.timeZone).date()];
    }
}

void StatsModel::rebuildCompletedDays()
{
    m_completedPerDay.clear();
    for (const qint64 ms : std::as_const(m_completedStarts)) {
        ++m_completedPerDay[QDateTime::fromMSecsSinceEpoch(ms, m_env.timeZone).date()];
    }
}

void StatsModel::recompute()
{
    if (!m_repository) {
        m_summary = StatsSummary();
        m_currentRecords.clear();
        m_completedStarts.clear();
        m_completedStartsMaxId = 0;
        m_completedPerDay.clear();
        m_bucketsVariant.clear();
        m_maxBucketSeconds = 3600;
        Q_EMIT statsChanged();
        return;
    }

    const StatsPeriod sp = toStatsPeriod(m_period);
    const PeriodRange range = Stats::rangeFor(sp, m_anchorDate, m_env.firstDayOfWeek, m_env.timeZone);

    m_currentRecords = m_repository->between(range.startMs, range.endMs);
    m_summary = Stats::summarize(m_currentRecords, sp, m_anchorDate, m_env.firstDayOfWeek,
                                 m_env.timeZone, m_env.locale, m_includeInterrupted, m_env.now());

    m_bucketsVariant.clear();
    qint64 highest = 0;

    for (const StatsBucket &b : m_summary.buckets) {
        highest = std::max({highest, b.workSeconds, b.breakSeconds});

        QString tooltip;
        switch (m_period) {
        case Period::Day:
            tooltip = i18n("%1:00 – %2:00\nWork: %3\nBreaks: %4",
                           b.hour, (b.hour + 1) % 24,
                           formatDuration(b.workSeconds),
                           formatDuration(b.breakSeconds));
            break;
        case Period::Week:
        case Period::Month:
            tooltip = i18n("%1\nWork: %2\nBreaks: %3",
                           m_env.locale.toString(b.date, QLocale::LongFormat),
                           formatDuration(b.workSeconds),
                           formatDuration(b.breakSeconds));
            break;
        case Period::Year:
            tooltip = i18n("%1\nWork: %2\nBreaks: %3",
                           m_env.locale.standaloneMonthName(b.date.month(), QLocale::LongFormat),
                           formatDuration(b.workSeconds),
                           formatDuration(b.breakSeconds));
            break;
        }

        QVariantMap map;
        map[QStringLiteral("label")] = b.label;
        map[QStringLiteral("workSeconds")] = b.workSeconds;
        map[QStringLiteral("breakSeconds")] = b.breakSeconds;
        map[QStringLiteral("totalSeconds")] = b.workSeconds + b.breakSeconds;
        map[QStringLiteral("isCurrent")] = b.isCurrent;
        map[QStringLiteral("tooltip")] = tooltip;
        map[QStringLiteral("hour")] = b.hour;
        map[QStringLiteral("date")] = b.date;

        m_bucketsVariant.append(map);
    }

    m_maxBucketSeconds = std::max<qint64>(highest, 3600);
    Q_EMIT statsChanged();
}

void StatsModel::recomputeStreaks()
{
    if (!m_repository) {
        m_currentStreak = 0;
        m_bestStreak = 0;
        m_todayCompletedCount = 0;
        Q_EMIT streaksChanged();
        Q_EMIT statsChanged();
        return;
    }

    // Works on the per-day cache: proportional to the number of active days, not sessions.
    QSet<QDate> days;
    days.reserve(m_completedPerDay.size());
    for (auto it = m_completedPerDay.cbegin(); it != m_completedPerDay.cend(); ++it) {
        days.insert(it.key());
    }
    // m_today is deliberately left alone: checkDateChange() compares against it to notice that the
    // day rolled over and to move the shown period, so updating it here would hide the change.
    const QDate today = todayDate();

    m_currentStreak = Stats::currentStreak(days, today, m_protectWeekendStreak);
    m_bestStreak = Stats::bestStreak(days, m_protectWeekendStreak);
    m_todayCompletedCount = m_completedPerDay.value(today, 0);

    Q_EMIT streaksChanged();
    Q_EMIT statsChanged();
}

void StatsModel::setDailyGoal(int goal)
{
    goal = std::max(goal, 0);
    if (m_dailyGoal == goal) {
        return;
    }
    m_dailyGoal = goal;
    Q_EMIT dailyGoalChanged();
    Q_EMIT statsChanged();
}

double StatsModel::dailyGoalProgress() const
{
    return m_dailyGoal > 0 ? qBound(0.0, double(m_todayCompletedCount) / double(m_dailyGoal), 1.0) : 0.0;
}

void StatsModel::setProtectWeekendStreak(bool protect)
{
    if (m_protectWeekendStreak == protect) {
        return;
    }
    m_protectWeekendStreak = protect;
    recomputeStreaks();
    Q_EMIT protectWeekendStreakChanged();
}

QVariantList StatsModel::breakdown() const
{
    QVariantList list;
    const QList<BreakdownEntry> *entriesPtr = nullptr;
    if (m_breakdownMode == BreakdownMode::ByTimer) {
        entriesPtr = &m_summary.byTimer;
    } else if (m_breakdownMode == BreakdownMode::ByCategory) {
        entriesPtr = &m_summary.byCategory;
    } else {
        entriesPtr = &m_summary.byTask;
    }
    const QList<BreakdownEntry> &entries = *entriesPtr;

    const qint64 totalWork = m_summary.workSeconds;

    for (const BreakdownEntry &entry : entries) {
        QVariantMap map;
        QString name = entry.name;
        if (name.isEmpty()) {
            if (m_breakdownMode == BreakdownMode::ByTimer) {
                name = i18n("Untitled Timer");
            } else if (m_breakdownMode == BreakdownMode::ByCategory) {
                name = i18n("Uncategorized");
            } else {
                name = i18n("Without note");
            }
        }
        map[QStringLiteral("name")] = name;
        map[QStringLiteral("workSeconds")] = entry.workSeconds;
        map[QStringLiteral("sessions")] = entry.sessions;
        map[QStringLiteral("formattedDuration")] = formatDuration(entry.workSeconds);

        const double pct = totalWork > 0 ? (double(entry.workSeconds) / double(totalWork)) * 100.0 : 0.0;
        map[QStringLiteral("percentage")] = pct;
        map[QStringLiteral("formattedPercentage")] = QStringLiteral("%1%").arg(QString::number(pct, 'f', 1));

        list.append(map);
    }

    return list;
}

bool StatsModel::exportCsv(const QUrl &fileUrl)
{
    auto fail = [this](const QString &message) {
        m_lastExportError = message;
        Q_EMIT exportFailed(message);
        return false;
    };

    // Only local files: a remote URL's text is not a path, and writing it as one would put a
    // file named like "sftp:/host/..." somewhere unexpected.
    if (!fileUrl.isLocalFile() || fileUrl.toLocalFile().isEmpty()) {
        return fail(i18n("Statistics can only be exported to a local file."));
    }
    const QString path = fileUrl.toLocalFile();

    // The same sessions the page shows: interrupted phases only while they are included.
    QList<SessionRecord> records;
    records.reserve(m_currentRecords.size());
    for (const SessionRecord &r : std::as_const(m_currentRecords)) {
        if (r.completed || m_includeInterrupted) {
            records.append(r);
        }
    }

    // QSaveFile writes to a temporary file and replaces the target only if everything,
    // including the final flush, succeeded; a failed export never leaves a truncated file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcStatsModel) << "Failed to open file for CSV export:" << path << file.errorString();
        return fail(i18n("Could not write %1: %2", QFileInfo(path).fileName(), file.errorString()));
    }
    if (!CsvExporter::write(file, records, m_env.timeZone)) {
        file.cancelWriting();
        qCWarning(lcStatsModel) << "Failed to write CSV export:" << path << file.errorString();
        return fail(i18n("Could not write %1: %2", QFileInfo(path).fileName(), file.errorString()));
    }
    if (!file.commit()) {
        qCWarning(lcStatsModel) << "Failed to save CSV export:" << path << file.errorString();
        return fail(i18n("Could not write %1: %2", QFileInfo(path).fileName(), file.errorString()));
    }
    m_lastExportError.clear();
    return true;
}

QString StatsModel::formatDuration(qint64 seconds) const
{
    if (seconds <= 0) {
        return i18n("0m");
    }
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;

    if (hours > 0) {
        if (minutes > 0) {
            return i18n("%1h %2m", hours, minutes);
        }
        return i18n("%1h", hours);
    }
    if (minutes > 0) {
        return i18n("%1m", minutes);
    }
    return i18n("%1s", secs);
}

QString StatsModel::formatRatio(double ratio) const
{
    if (ratio < 0) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1 : 1").arg(QString::number(ratio, 'f', 1));
}
