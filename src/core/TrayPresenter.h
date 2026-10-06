// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include "TimerEngine.h"

/// What the tray icon should say for a given timer state.
struct TrayStatus {
    QString title;         ///< Short, e.g. for task-manager style listings.
    QString toolTipTitle;
    QString toolTipText;
    int badgeMinutes = -1; ///< Minutes to draw on the icon while a phase runs or is paused; -1 = none.
};

/// "m:ss", or "h:mm:ss" from one hour up.
QString formatClock(int totalSeconds);

/// Pure function of the timer state, so the wording and the badge are easy to test.
TrayStatus describeTray(TimerEngine::State state, TimerEngine::Phase phase, int remainingSeconds, const QString &presetName);
