// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>

#include "TimerEngine.h"
#include "SessionRepository.h"
#include "TimerPreset.h"

class PresetModel;

/**
 * Stores every finished phase of the TimerEngine.
 *
 * The preset is captured when a phase *starts*, not when it ends, so a session is
 * always attributed to the timer it ran with even if the user switches timers
 * (which stops the running phase) or edits the preset afterwards.
 *
 * Phases cut short by stop/skip are kept only if they lasted at least
 * kMinInterruptedMs, so accidental starts do not clutter the statistics.
 */
class SessionRecorder : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 kMinInterruptedMs = 5000;

    SessionRecorder(TimerEngine *engine, PresetModel *presets, SessionRepository *repository, QObject *parent = nullptr);

    /// Row id of the most recently ended Work phase, or -1 if that phase was not recorded
    /// (too short, or the insert failed) or no Work phase has ended yet. Breaks leave it as is,
    /// so a note typed during the following break still lands on the right session.
    qint64 lastWorkSessionId() const { return m_lastWorkSessionId; }

    /// Forgets that id. Called when the stored history is replaced (cleared or restored), so a
    /// later note cannot land on an unrelated row that happens to reuse the id.
    void forgetLastWorkSession() { m_lastWorkSessionId = -1; }

Q_SIGNALS:
    void sessionRecorded();
    /// A Work phase ran to its end and was stored; record.id is its new row id.
    void workSessionRecorded(const SessionRecord &record);

private Q_SLOTS:
    void onPhaseStarted(TimerEngine::Phase phase);
    void onSessionEnded(TimerEngine::Phase phase,
                        const QDateTime &startedAt,
                        const QDateTime &endedAt,
                        qint64 activeMs,
                        qint64 plannedMs,
                        bool completed);

private:
    PresetModel *m_presets;
    SessionRepository *m_repository;
    TimerPreset m_active; ///< Preset captured at the start of the running phase.
    qint64 m_lastWorkSessionId = -1;
};
