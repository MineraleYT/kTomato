// SPDX-License-Identifier: GPL-3.0-or-later
#include "FreedesktopInhibitor.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLoggingCategory>
#include <QVariantMap>

Q_LOGGING_CATEGORY(lcInhibitor, "ktomato.platform.inhibitor")

namespace
{
const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
const QString kInterface = QStringLiteral("org.freedesktop.Notifications");
constexpr int kCallTimeoutMs = 3000;
} // namespace

FreedesktopInhibitor::FreedesktopInhibitor(const QDBusConnection &bus, const QString &appId, QObject *parent)
    : NotificationInhibitor(parent)
    , m_bus(bus)
    , m_appId(appId)
    , m_watcher(kService, m_bus, QDBusServiceWatcher::WatchForOwnerChange)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, &FreedesktopInhibitor::onOwnerChanged);
}

void FreedesktopInhibitor::inhibit(const QString &reason)
{
    m_wanted = true;
    m_reason = reason;
    sync();
}

void FreedesktopInhibitor::release()
{
    m_wanted = false;
    sync();
}

// The whole behaviour: with no call in flight, make the server match what is wanted.
// Every completed call ends by running this again, so a change of mind during a call
// is picked up as soon as the call returns.
void FreedesktopInhibitor::sync()
{
    if (m_busy) {
        return;
    }
    if (m_wanted && !m_hasCookie) {
        QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Inhibit"));
        message.setArguments({m_appId, m_reason, QVariant::fromValue(QVariantMap())});
        startCall(message, &FreedesktopInhibitor::onInhibitFinished);
    } else if (!m_wanted && m_hasCookie) {
        QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("UnInhibit"));
        message.setArguments({m_cookie});
        startCall(message, &FreedesktopInhibitor::onUnInhibitFinished);
    }
}

void FreedesktopInhibitor::startCall(const QDBusMessage &message, void (FreedesktopInhibitor::*handler)(QDBusPendingCallWatcher *))
{
    m_busy = true;
    report();
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, kCallTimeoutMs), this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, handler, generation](QDBusPendingCallWatcher *w) {
        if (generation != m_generation) {
            // Sent to a notification server that has since gone away: its answer (a stale
            // cookie or an error) says nothing about the current server. onOwnerChanged()
            // already reset the state, so just drop it.
            w->deleteLater();
            return;
        }
        (this->*handler)(w);
    });
}

void FreedesktopInhibitor::onInhibitFinished(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    QDBusPendingReply<quint32> reply = *watcher;
    m_busy = false;

    if (reply.isError()) {
        qCWarning(lcInhibitor) << "Cannot silence notifications:" << reply.error().message();
        // Give up on this request instead of retrying in a loop; the next inhibit() tries again.
        m_wanted = false;
        report();
        return;
    }

    m_cookie = reply.value();
    m_hasCookie = true;
    report();
    sync(); // release() may have been called while the call was in flight
}

void FreedesktopInhibitor::onUnInhibitFinished(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    QDBusPendingReply<> reply = *watcher;
    m_busy = false;

    if (reply.isError()) {
        // The server no longer knows the cookie (it restarted) or is gone; either way nothing
        // of ours is held any more, because the server drops inhibitions with the connection.
        qCWarning(lcInhibitor) << "UnInhibit failed:" << reply.error().message();
    }
    m_hasCookie = false;
    report();
    sync(); // inhibit() may have been called again while the call was in flight
}

// The notification service restarted (for example plasmashell): inhibitions died with it.
void FreedesktopInhibitor::onOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    ++m_generation; // abandon any call still in flight to the old owner
    m_busy = false;
    m_hasCookie = false;
    report();
    if (!newOwner.isEmpty()) {
        sync(); // still wanted: take the inhibition again on the new server
    }
}

void FreedesktopInhibitor::report()
{
    const bool now = isInhibited();
    if (now != m_reported) {
        m_reported = now;
        Q_EMIT inhibitedChanged();
    }
}
