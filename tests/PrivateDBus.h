// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QProcess>
#include <QString>

/// A throw-away message bus, so D-Bus tests never touch the user's real session.
class PrivateDBus
{
public:
    ~PrivateDBus() { stop(); }

    /// False if dbus-daemon is not installed (tests then skip).
    bool start()
    {
        m_daemon.start(QStringLiteral("dbus-daemon"),
                       {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
        if (!m_daemon.waitForStarted(3000) || !m_daemon.waitForReadyRead(5000)) {
            return false;
        }
        m_address = QString::fromLocal8Bit(m_daemon.readLine().trimmed());
        return !m_address.isEmpty();
    }

    void stop()
    {
        if (m_daemon.state() != QProcess::NotRunning) {
            m_daemon.terminate();
            m_daemon.waitForFinished(3000);
        }
    }

    QString address() const { return m_address; }

private:
    QProcess m_daemon;
    QString m_address;
};
