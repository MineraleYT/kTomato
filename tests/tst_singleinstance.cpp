// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "PrivateDBus.h"
#include "SingleInstance.h"

namespace
{
const QString kService = QStringLiteral("org.example.ktomato.test");
}

class SingleInstanceTest : public QObject
{
    Q_OBJECT

private:
    PrivateDBus bus;

    QDBusConnection first() const { return QDBusConnection(QStringLiteral("first")); }
    QDBusConnection second() const { return QDBusConnection(QStringLiteral("second")); }

private Q_SLOTS:
    void initTestCase()
    {
        if (!bus.start()) {
            QSKIP("dbus-daemon is not available");
        }
        QDBusConnection::connectToBus(bus.address(), QStringLiteral("first"));
        QDBusConnection::connectToBus(bus.address(), QStringLiteral("second"));
        QVERIFY(first().isConnected());
        QVERIFY(second().isConnected());
        QLoggingCategory::setFilterRules(QStringLiteral("ktomato.platform.singleinstance.warning=false"));
    }

    void cleanupTestCase()
    {
        QDBusConnection::disconnectFromBus(QStringLiteral("first"));
        QDBusConnection::disconnectFromBus(QStringLiteral("second"));
    }

    void theFirstLaunchBecomesThePrimaryInstance()
    {
        SingleInstance primary(first(), kService);
        QVERIFY(primary.acquire());
        QVERIFY(first().interface()->isServiceRegistered(kService));
    }

    void aSecondLaunchAsksTheFirstToShowItselfAndShouldExit()
    {
        SingleInstance primary(first(), kService);
        QVERIFY(primary.acquire());
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);

        SingleInstance later(second(), kService);
        QVERIFY(!later.acquire()); // "exit, the other one has it"

        QTRY_COMPARE(activated.count(), 1);
    }

    void theActivationTokenReachesTheFirstInstance()
    {
        SingleInstance primary(first(), kService);
        QVERIFY(primary.acquire());
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);

        qputenv("XDG_ACTIVATION_TOKEN", "token-123");
        SingleInstance later(second(), kService);
        QVERIFY(!later.acquire());
        qunsetenv("XDG_ACTIVATION_TOKEN");

        QTRY_COMPARE(activated.count(), 1);
        QCOMPARE(activated.first().first().toString(), QStringLiteral("token-123"));
    }

    void everyExtraLaunchActivatesAgain()
    {
        SingleInstance primary(first(), kService);
        QVERIFY(primary.acquire());
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);

        for (int i = 0; i < 3; ++i) {
            SingleInstance later(second(), kService);
            QVERIFY(!later.acquire());
        }
        QTRY_COMPARE(activated.count(), 3);
    }

    void aBackgroundLaunchDoesNotShowTheWindow()
    {
        SingleInstance primary(first(), kService);
        QVERIFY(primary.acquire());
        QSignalSpy activated(&primary, &SingleInstance::activateRequested);

        SingleInstance later(second(), kService);
        QVERIFY(!later.acquire(false)); // still "exit", but quietly
        QTest::qWait(200);
        QCOMPARE(activated.count(), 0);
    }

    void aRequestBeforeAnyoneListensIsDeliveredOnConnect()
    {
        SingleInstance primary(first(), kService);
        QVERIFY(primary.acquire());

        qputenv("XDG_ACTIVATION_TOKEN", "early");
        SingleInstance later(second(), kService);
        QVERIFY(!later.acquire());
        qunsetenv("XDG_ACTIVATION_TOKEN");

        QSignalSpy activated(&primary, &SingleInstance::activateRequested);
        QTRY_COMPARE(activated.count(), 1);
        QCOMPARE(activated.first().first().toString(), QStringLiteral("early"));
    }

    void theNameIsFreeAgainWhenThePrimaryInstanceGoesAway()
    {
        {
            SingleInstance primary(first(), kService);
            QVERIFY(primary.acquire());
        }
        SingleInstance next(second(), kService);
        QVERIFY(next.acquire());
    }

    void withoutASessionBusThereIsNothingToCoordinateWith()
    {
        SingleInstance alone(QDBusConnection(QStringLiteral("no-such-connection")), kService);
        QVERIFY(alone.acquire());
    }
};

QTEST_GUILESS_MAIN(SingleInstanceTest)
#include "tst_singleinstance.moc"
