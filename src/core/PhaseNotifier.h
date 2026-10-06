// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

#include <functional>

#include "TimerEngine.h"

class NotificationInhibitor;
class PresetModel;
class AppSettings;

/**
 * Tells the user that a phase ended, honouring the current preset's
 * "notification" and "sound" options.
 *
 * Three KNotification events (see data/ktomato.notifyrc) cover the option
 * combinations, so Plasma's per-event notification settings keep working.
 *
 * When a work phase ends while notifications are silenced, the notification is held
 * back until the silencing has really ended; otherwise the desktop would swallow the
 * very message announcing the break.
 *
 * The actions of a notification only act while the timer is still where the notification
 * left it: once another phase has started, they do nothing (a late "Skip Break" never skips
 * the following work phase).
 */
class PhaseNotifier : public QObject
{
    Q_OBJECT

public:
    PhaseNotifier(TimerEngine *engine, PresetModel *presets, NotificationInhibitor *inhibitor, AppSettings *settings = nullptr, QObject *parent = nullptr);

    /// Returns the id of the work session recorded last (<= 0 if unknown). Asked when a work
    /// phase ends; without it, notifications do not offer to log a task note.
    void setLastWorkSessionIdProvider(std::function<qint64()> provider);

Q_SIGNALS:
    /// The user wants to write a note for the work session `sessionId`.
    void taskNotePromptRequested(const QString &presetName, qint64 sessionId);
    /// The user wrote `note` for the work session `sessionId` straight in the notification.
    void taskNoteSubmitted(qint64 sessionId, const QString &note);
    /// The notification was clicked and there is nothing more specific to do: show the window.
    void showWindowRequested();

private Q_SLOTS:
    void onPhaseFinished(TimerEngine::Phase finished, TimerEngine::Phase next);
    void onOneMinuteRemaining(TimerEngine::Phase phase);

private:
    TimerEngine *m_engine;
    PresetModel *m_presets;
    NotificationInhibitor *m_inhibitor;
    AppSettings *m_settings;
    std::function<qint64()> m_lastWorkSessionId;
    quint64 m_phaseSerial = 0; ///< Counts phaseStarted(), so stale notification actions can be ignored.
};
