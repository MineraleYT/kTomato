// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusAbstractAdaptor>
#include <QObject>

#include "TimerEngine.h"

/**
 * Exposes TimerEngine controls over D-Bus at object path /Timer
 * with interface io.github.mineraleyt.ktomato.Timer.
 *
 * This allows KDE Plasma Global Shortcuts, command line calls (qdbus),
 * scripts, and hardware buttons to start, pause, skip, or stop the timer.
 */
class DBusTimerService : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.mineraleyt.ktomato.Timer")

public:
    explicit DBusTimerService(TimerEngine *engine, QObject *parent = nullptr);

public Q_SLOTS:
    void toggle();
    void start();
    void pause();
    void stop();
    void skip();
    QString status() const;

private:
    TimerEngine *m_engine;
};
