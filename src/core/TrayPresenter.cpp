// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrayPresenter.h"

#include <KLocalizedString>

QString formatClock(int totalSeconds)
{
    totalSeconds = qMax(0, totalSeconds);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

namespace
{
QString phaseName(TimerEngine::Phase phase)
{
    switch (phase) {
    case TimerEngine::Phase::Work:
        return i18n("Work");
    case TimerEngine::Phase::ShortBreak:
        return i18n("Short break");
    case TimerEngine::Phase::LongBreak:
        return i18n("Long break");
    }
    Q_UNREACHABLE_RETURN(QString());
}
} // namespace

TrayStatus describeTray(TimerEngine::State state, TimerEngine::Phase phase, int remainingSeconds, const QString &presetName)
{
    TrayStatus status;
    const QString clock = formatClock(remainingSeconds);
    const QString phaseLabel = phaseName(phase);

    switch (state) {
    case TimerEngine::State::Idle:
        status.title = i18n("kTomato");
        status.toolTipTitle = i18n("kTomato: ready");
        status.toolTipText = i18n("Next: %1, %2\n%3", phaseLabel, clock, presetName);
        break;
    case TimerEngine::State::Paused:
        status.title = i18n("kTomato: paused");
        status.toolTipTitle = i18n("%1 (paused)", phaseLabel);
        status.toolTipText = i18n("%1 left\n%2", clock, presetName);
        status.badgeMinutes = (remainingSeconds + 59) / 60;
        break;
    default:
        status.title = i18n("kTomato: %1", phaseLabel);
        status.toolTipTitle = phaseLabel;
        status.toolTipText = i18n("%1 left\n%2", clock, presetName);
        status.badgeMinutes = (remainingSeconds + 59) / 60;
        break;
    }
    return status;
}
