// SPDX-License-Identifier: GPL-3.0-or-later
#include "TimerEngine.h"

#include <algorithm>

namespace
{
constexpr int kTickIntervalMs = 100;
}

TimerEngine::TimerEngine(QObject *parent)
    : TimerEngine(nullptr, parent)
{
}

TimerEngine::TimerEngine(Clock *clock, QObject *parent)
    : QObject(parent)
{
    if (clock) {
        m_clock = clock;
    } else {
        m_ownedClock = std::make_unique<SystemClock>();
        m_clock = m_ownedClock.get();
    }

    m_ticker.setInterval(kTickIntervalMs);
    m_ticker.setTimerType(Qt::PreciseTimer);
    connect(&m_ticker, &QTimer::timeout, this, &TimerEngine::refresh);
}

// --- Queries ---------------------------------------------------------------

bool TimerEngine::isRunning() const
{
    return m_state == State::Working || m_state == State::ShortBreak || m_state == State::LongBreak;
}

qint64 TimerEngine::durationMs(Phase phase) const
{
    switch (phase) {
    case Phase::Work:
        return m_workSeconds * 1000LL;
    case Phase::ShortBreak:
        return m_shortBreakSeconds * 1000LL;
    case Phase::LongBreak:
        return m_longBreakSeconds * 1000LL;
    }
    Q_UNREACHABLE_RETURN(0);
}

qint64 TimerEngine::remainingMs() const
{
    switch (m_state) {
    case State::Idle:
        return durationMs(m_phase);
    case State::Paused:
        return m_remainingAtPauseMs;
    default:
        return std::max<qint64>(0, m_deadlineMs - m_clock->monotonicMs());
    }
}

int TimerEngine::remainingSeconds() const
{
    return static_cast<int>((remainingMs() + 999) / 1000); // round up: 0:00 only when truly done
}

int TimerEngine::totalSeconds() const
{
    const qint64 total = (m_state == State::Idle) ? durationMs(m_phase) : m_plannedMs;
    return static_cast<int>(total / 1000);
}

double TimerEngine::progress() const
{
    if (m_state == State::Idle || m_plannedMs <= 0) {
        return 0.0;
    }
    return std::clamp(1.0 - double(remainingMs()) / double(m_plannedMs), 0.0, 1.0);
}

TimerEngine::State TimerEngine::stateFor(Phase phase)
{
    switch (phase) {
    case Phase::Work:
        return State::Working;
    case Phase::ShortBreak:
        return State::ShortBreak;
    case Phase::LongBreak:
        return State::LongBreak;
    }
    Q_UNREACHABLE_RETURN(State::Idle);
}

// --- Commands --------------------------------------------------------------

void TimerEngine::start()
{
    if (m_state == State::Idle) {
        startPhase(m_phase);
    }
}

void TimerEngine::pause()
{
    if (!isRunning()) {
        return;
    }
    m_remainingAtPauseMs = remainingMs();
    m_ticker.stop();
    setState(State::Paused);
    notifyTime(false);
}

void TimerEngine::resume()
{
    if (m_state != State::Paused) {
        return;
    }
    m_deadlineMs = m_clock->monotonicMs() + m_remainingAtPauseMs;
    setState(stateFor(m_phase));
    m_ticker.start();
    notifyTime(false);
}

void TimerEngine::toggle()
{
    switch (m_state) {
    case State::Idle:
        start();
        break;
    case State::Paused:
        resume();
        break;
    default:
        pause();
        break;
    }
}

void TimerEngine::stop()
{
    if (m_state == State::Idle) {
        return;
    }
    emitSessionEnded(false, m_clock->wallTime());
    setCycles(0);
    setIdle(Phase::Work);
}

void TimerEngine::reset()
{
    if (m_state != State::Idle) {
        stop();
        return;
    }
    setCycles(0);
    setIdle(Phase::Work);
}

void TimerEngine::skip()
{
    if (m_state == State::Idle) {
        // Nothing runs, so nothing is recorded: just move on to what would follow the pending phase.
        setIdle(nextPhaseAfter(m_phase));
        return;
    }
    emitSessionEnded(false, m_clock->wallTime());
    startPhase(nextPhaseAfter(m_phase));
}

void TimerEngine::startCustomWork(int seconds)
{
    if (m_state != State::Idle) {
        emitSessionEnded(false, m_clock->wallTime());
    }
    startPhase(Phase::Work, std::max(1, seconds) * 1000LL);
}

void TimerEngine::refresh()
{
    if (!isRunning()) {
        return;
    }
    const qint64 now = m_clock->monotonicMs();
    if (now >= m_deadlineMs) {
        completePhase(now);
    } else {
        notifyTime(false);
    }
}

// --- Transitions -----------------------------------------------------------

void TimerEngine::startPhase(Phase phase, qint64 plannedMs)
{
    if (phase != Phase::Work && durationMs(phase) <= 0) {
        // A zero-length break means "no break": go straight back to work. Normally handled by
        // nextPhaseAfter(); this covers a pending break whose duration was set to 0 meanwhile.
        if (phase == Phase::LongBreak) {
            setCycles(0);
        }
        phase = Phase::Work;
    }

    setPhase(phase);
    m_plannedMs = plannedMs >= 0 ? plannedMs : durationMs(phase);
    m_startWall = m_clock->wallTime();
    m_deadlineMs = m_clock->monotonicMs() + m_plannedMs;
    m_warnedOneMinute = false;
    setState(stateFor(phase));
    m_ticker.start();
    notifyTime(true);
    Q_EMIT phaseStarted(phase);
}

void TimerEngine::setIdle(Phase next)
{
    m_ticker.stop();
    setPhase(next);
    setState(State::Idle);
    m_warnedOneMinute = false;
    notifyTime(true);
}

void TimerEngine::completePhase(qint64 nowMs)
{
    // If the tick arrived late, the phase really ended at the deadline.
    const qint64 lateMs = nowMs - m_deadlineMs;
    const Phase finished = m_phase;

    m_ticker.stop();
    emitSessionEnded(true, m_clock->wallTime().addMSecs(-lateMs));

    const Phase next = nextPhaseAfter(finished);
    // Start the next phase before announcing the end, so listeners see the real state. Going
    // straight from one running state to the next also avoids a passing Idle state, which would
    // make the power/notification inhibitors and the focus sound let go and grab again.
    if (shouldAutoStart(next)) {
        startPhase(next);
    } else {
        setIdle(next);
    }
    Q_EMIT phaseFinished(finished, next);
}

bool TimerEngine::shouldAutoStart(Phase next) const
{
    switch (m_autoStartMode) {
    case AutoStartMode::All:
        return true;
    case AutoStartMode::BreaksOnly:
        return next != Phase::Work;
    case AutoStartMode::Manual:
        break;
    }
    return false;
}

void TimerEngine::emitSessionEnded(bool completed, const QDateTime &endedAt)
{
    const qint64 activeMs = completed ? m_plannedMs : m_plannedMs - remainingMs();
    Q_EMIT sessionEnded(m_phase, m_startWall, endedAt, activeMs, m_plannedMs, completed);
}

TimerEngine::Phase TimerEngine::nextPhaseAfter(Phase finished)
{
    switch (finished) {
    case Phase::Work: {
        setCycles(m_completedCycles + 1);
        const Phase brk = (m_cyclesBeforeLong > 0 && m_completedCycles >= m_cyclesBeforeLong) ? Phase::LongBreak
                                                                                              : Phase::ShortBreak;
        if (durationMs(brk) > 0) {
            return brk;
        }
        // No break configured: back to work. A passed-over long break still ends the set.
        if (brk == Phase::LongBreak) {
            setCycles(0);
        }
        return Phase::Work;
    }
    case Phase::ShortBreak:
        return Phase::Work;
    case Phase::LongBreak:
        setCycles(0);
        return Phase::Work;
    }
    Q_UNREACHABLE_RETURN(Phase::Work);
}

// --- Change notification helpers -------------------------------------------

void TimerEngine::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged();
    }
}

void TimerEngine::setPhase(Phase phase)
{
    if (m_phase != phase) {
        m_phase = phase;
        Q_EMIT phaseChanged();
    }
}

void TimerEngine::setCycles(int cycles)
{
    if (m_completedCycles != cycles) {
        m_completedCycles = cycles;
        Q_EMIT cyclesChanged();
    }
}

void TimerEngine::notifyTime(bool force)
{
    const int seconds = remainingSeconds();
    if (force || seconds != m_lastRemainingSeconds) {
        m_lastRemainingSeconds = seconds;
        Q_EMIT remainingChanged();

        // Only phases longer than the warning itself get one; otherwise it would fire at the start.
        if (m_state != State::Idle && m_plannedMs > 60 * 1000 && seconds > 0 && seconds <= 60 && !m_warnedOneMinute) {
            m_warnedOneMinute = true;
            Q_EMIT oneMinuteRemaining(m_phase);
        }
    }
    if (force) {
        Q_EMIT totalChanged();
    }
    Q_EMIT progressChanged();
}

void TimerEngine::refreshIdleDisplay()
{
    if (m_state == State::Idle) {
        notifyTime(true);
    }
}

// --- Configuration ---------------------------------------------------------

void TimerEngine::setWorkSeconds(int seconds)
{
    seconds = std::max(1, seconds);
    if (m_workSeconds != seconds) {
        m_workSeconds = seconds;
        Q_EMIT workSecondsChanged();
        refreshIdleDisplay();
    }
}

void TimerEngine::setShortBreakSeconds(int seconds)
{
    seconds = std::max(0, seconds);
    if (m_shortBreakSeconds != seconds) {
        m_shortBreakSeconds = seconds;
        Q_EMIT shortBreakSecondsChanged();
        refreshIdleDisplay();
    }
}

void TimerEngine::setLongBreakSeconds(int seconds)
{
    seconds = std::max(0, seconds);
    if (m_longBreakSeconds != seconds) {
        m_longBreakSeconds = seconds;
        Q_EMIT longBreakSecondsChanged();
        refreshIdleDisplay();
    }
}

void TimerEngine::setCyclesBeforeLong(int cycles)
{
    cycles = std::max(0, cycles);
    if (m_cyclesBeforeLong != cycles) {
        m_cyclesBeforeLong = cycles;
        Q_EMIT cyclesBeforeLongChanged();
    }
}

void TimerEngine::setAutoStart(bool autoStart)
{
    setAutoStartMode(autoStart ? AutoStartMode::All : AutoStartMode::Manual);
}

void TimerEngine::setAutoStartMode(AutoStartMode mode)
{
    if (m_autoStartMode != mode) {
        const bool oldAuto = autoStart();
        m_autoStartMode = mode;
        Q_EMIT autoStartModeChanged();
        if (oldAuto != autoStart()) {
            Q_EMIT autoStartChanged();
        }
    }
}

void TimerEngine::setAutoStartModeInt(int mode)
{
    setAutoStartMode(static_cast<AutoStartMode>(std::clamp(mode, 0, 2)));
}
