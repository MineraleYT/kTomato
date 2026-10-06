// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScreenLockWatcher.h"

#include "AppSettings.h"
#include "TimerEngine.h"

#include <QLoggingCategory>

namespace
{
Q_LOGGING_CATEGORY(lcScreenLock, "ktomato.platform.screenlock")
const QString kInterface = QStringLiteral("org.freedesktop.ScreenSaver");
const QString kSignal = QStringLiteral("ActiveChanged");
} // namespace

ScreenLockWatcher::ScreenLockWatcher(QObject *parent)
    : ScreenLockWatcher(QDBusConnection::sessionBus(), parent)
{
}

ScreenLockWatcher::ScreenLockWatcher(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
    // Plasma emits ActiveChanged on both paths; onActiveChanged() drops the repeat.
    if (m_bus.isConnected()) {
        m_bus.connect(QString(),
                      QStringLiteral("/org/freedesktop/ScreenSaver"),
                      kInterface,
                      kSignal,
                      this,
                      SLOT(onActiveChanged(bool)));
        m_bus.connect(QString(),
                      QStringLiteral("/ScreenSaver"),
                      kInterface,
                      kSignal,
                      this,
                      SLOT(onActiveChanged(bool)));
    }
}

ScreenLockWatcher::~ScreenLockWatcher() = default;

void ScreenLockWatcher::attach(TimerEngine *engine, AppSettings *settings)
{
    m_engine = engine;
    m_settings = settings;

    if (m_engine) {
        connect(m_engine, &TimerEngine::stateChanged, this, [this]() {
            if (m_engine && m_engine->state() != TimerEngine::State::Paused && m_pausedByScreenLock) {
                m_pausedByScreenLock = false;
                Q_EMIT pausedByScreenLockChanged();
            }
        });
    }
}

void ScreenLockWatcher::clearLockPauseNotice()
{
    if (m_pausedByScreenLock) {
        m_pausedByScreenLock = false;
        Q_EMIT pausedByScreenLockChanged();
    }
}

void ScreenLockWatcher::onActiveChanged(bool active)
{
    if (active == m_screenLocked) {
        return; // the same change, reported again on the other object path
    }
    m_screenLocked = active;
    qCDebug(lcScreenLock) << "Screen lock state changed:" << active;

    if (active) {
        if (m_settings && m_settings->autoPauseOnScreenLock() && m_engine) {
            if (m_engine->state() == TimerEngine::State::Working) {
                qCDebug(lcScreenLock) << "Auto-pausing active timer due to screen lock";
                m_pausedByScreenLock = true;
                Q_EMIT pausedByScreenLockChanged();
                m_engine->pause();
            }
        }
    } else {
        if (m_pausedByScreenLock) {
            qCDebug(lcScreenLock) << "Screen unlocked after auto-pause";
            Q_EMIT screenUnlockedAfterPause();
        }
    }
}
