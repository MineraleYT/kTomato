// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class AppSettings;
class TimerEngine;

/**
 * Listens for system screen lock/unlock signals via FreeDesktop ScreenSaver D-Bus.
 * Automatically pauses the active work phase when the screen is locked, and
 * signals when the user returns so they can resume or review their session.
 */
class ScreenLockWatcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool pausedByScreenLock READ isPausedByScreenLock NOTIFY pausedByScreenLockChanged FINAL)

public:
    explicit ScreenLockWatcher(QObject *parent = nullptr);
    ScreenLockWatcher(const QDBusConnection &bus, QObject *parent = nullptr);
    ~ScreenLockWatcher() override;

    void attach(TimerEngine *engine, AppSettings *settings);

    bool isPausedByScreenLock() const { return m_pausedByScreenLock; }

    Q_INVOKABLE void clearLockPauseNotice();

public Q_SLOTS:
    void onActiveChanged(bool active);

Q_SIGNALS:
    void pausedByScreenLockChanged();
    void screenUnlockedAfterPause();

private:
    QDBusConnection m_bus;
    TimerEngine *m_engine = nullptr;
    AppSettings *m_settings = nullptr;
    bool m_pausedByScreenLock = false;
    bool m_screenLocked = false; // last state seen, to ignore duplicate signals
};
