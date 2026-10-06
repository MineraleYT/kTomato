// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

class QDBusServiceWatcher;
class TimerEngine;
class AppSettings;

/// Prevents the screen from dimming/sleeping and system idle standby during work phases.
///
/// Inhibit() answers asynchronously, so the wanted state can change while a request is in
/// flight: at most one request is outstanding, and a cookie that arrives when inhibition is
/// no longer wanted is handed straight back. If the screensaver service restarts, the old
/// cookie is dropped and inhibition is requested again from the new owner.
class PowerInhibitor : public QObject
{
    Q_OBJECT

public:
    explicit PowerInhibitor(const QDBusConnection &bus = QDBusConnection::sessionBus(),
                            QObject *parent = nullptr);
    ~PowerInhibitor() override;

    void attach(TimerEngine *engine, AppSettings *settings);

    bool isInhibited() const { return m_cookie != 0; }

public Q_SLOTS:
    void inhibit();
    void release();
    void update();

private:
    void sync();
    void sendInhibit();
    void sendUnInhibit(uint cookie);
    void onOwnerChanged(const QString &newOwner);

    QDBusConnection m_bus;
    QDBusServiceWatcher *m_watcher = nullptr;
    TimerEngine *m_engine = nullptr;
    AppSettings *m_settings = nullptr;
    uint m_cookie = 0;
    bool m_wanted = false;
    bool m_pending = false;     // an Inhibit call is waiting for its reply
    quint64 m_generation = 0;   // bumped when the service owner changes
};
