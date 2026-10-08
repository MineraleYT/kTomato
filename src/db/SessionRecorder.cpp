// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionRecorder.h"

#include "PresetModel.h"
#include "SessionRepository.h"

namespace
{
SessionKind kindFor(TimerEngine::Phase phase)
{
    switch (phase) {
    case TimerEngine::Phase::Work:
        return SessionKind::Work;
    case TimerEngine::Phase::ShortBreak:
        return SessionKind::ShortBreak;
    case TimerEngine::Phase::LongBreak:
        return SessionKind::LongBreak;
    }
    Q_UNREACHABLE_RETURN(SessionKind::Work);
}
} // namespace

SessionRecorder::SessionRecorder(TimerEngine *engine, PresetModel *presets, SessionRepository *repository, QObject *parent)
    : QObject(parent)
    , m_presets(presets)
    , m_repository(repository)
    , m_active(presets->currentPreset())
{
    connect(engine, &TimerEngine::phaseStarted, this, &SessionRecorder::onPhaseStarted);
    connect(engine, &TimerEngine::sessionEnded, this, &SessionRecorder::onSessionEnded);
}

void SessionRecorder::onPhaseStarted(TimerEngine::Phase)
{
    m_active = m_presets->currentPreset();
}

void SessionRecorder::onSessionEnded(TimerEngine::Phase phase,
                                     const QDateTime &startedAt,
                                     const QDateTime &endedAt,
                                     qint64 activeMs,
                                     qint64 plannedMs,
                                     bool completed)
{
    const bool isWork = phase == TimerEngine::Phase::Work;
    if (!completed && activeMs < kMinInterruptedMs) {
        if (isWork) {
            m_lastWorkSessionId = -1;
        }
        return;
    }

    SessionRecord record;
    record.kind = kindFor(phase);
    record.startedAtMs = startedAt.toMSecsSinceEpoch();
    record.endedAtMs = endedAt.toMSecsSinceEpoch();
    record.durationSec = int((activeMs + 500) / 1000);
    record.plannedSec = int((plannedMs + 500) / 1000);
    record.completed = completed;
    record.presetUuid = m_active.uuid;
    record.presetName = m_active.name;
    record.category = m_active.category;
    qint64 id = -1;
    const bool inserted = m_repository->insert(record, &id);
    if (isWork) {
        m_lastWorkSessionId = inserted ? id : -1;
    }
    if (inserted) {
        Q_EMIT sessionRecorded();
        if (isWork && completed) {
            record.id = id;
            Q_EMIT workSessionRecorded(record);
        }
    }
}
