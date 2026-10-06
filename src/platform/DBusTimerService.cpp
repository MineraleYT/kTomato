// SPDX-License-Identifier: GPL-3.0-or-later
#include "DBusTimerService.h"

DBusTimerService::DBusTimerService(TimerEngine *engine, QObject *parent)
    : QDBusAbstractAdaptor(parent)
    , m_engine(engine)
{
}

void DBusTimerService::toggle()
{
    if (m_engine) {
        m_engine->toggle();
    }
}

void DBusTimerService::start()
{
    if (m_engine) {
        m_engine->start();
    }
}

void DBusTimerService::pause()
{
    if (m_engine) {
        m_engine->pause();
    }
}

void DBusTimerService::stop()
{
    if (m_engine) {
        m_engine->stop();
    }
}

void DBusTimerService::skip()
{
    if (m_engine) {
        m_engine->skip();
    }
}

QString DBusTimerService::status() const
{
    if (!m_engine) {
        return QStringLiteral("unavailable");
    }
    QString stateStr;
    if (m_engine->state() == TimerEngine::State::Idle) {
        stateStr = QStringLiteral("idle");
    } else if (m_engine->isPaused()) {
        stateStr = QStringLiteral("paused");
    } else {
        stateStr = QStringLiteral("running");
    }

    QString phaseStr;
    switch (m_engine->phase()) {
    case TimerEngine::Phase::Work:
        phaseStr = QStringLiteral("work");
        break;
    case TimerEngine::Phase::ShortBreak:
        phaseStr = QStringLiteral("short_break");
        break;
    case TimerEngine::Phase::LongBreak:
        phaseStr = QStringLiteral("long_break");
        break;
    }

    return QStringLiteral("%1:%2:%3").arg(stateStr, phaseStr, QString::number(m_engine->remainingSeconds()));
}
