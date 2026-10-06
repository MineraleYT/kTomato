// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "UpdateChecker.h"

class UpdateCheckerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compareVersionsSame()
    {
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.0.2"), QStringLiteral("1.0.2")), 0);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.0.2"), QStringLiteral("1.0.2")), 0);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.0"), QStringLiteral("1.0.0")), 0);
    }

    void compareVersionsNewer()
    {
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.0.3"), QStringLiteral("v1.0.2")), 1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0"), QStringLiteral("1.0.9")), 1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("2.0.0"), QStringLiteral("1.99.9")), 1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.0.2.1"), QStringLiteral("v1.0.2")), 1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.0.3-beta"), QStringLiteral("1.0.2")), 1);
    }

    void compareVersionsOlder()
    {
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.0.1"), QStringLiteral("v1.0.2")), -1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("0.9.9"), QStringLiteral("1.0.0")), -1);
    }

    void initialPropertiesAndReset()
    {
        UpdateChecker checker;
        QCOMPARE(checker.status(), UpdateChecker::Status::Idle);
        QVERIFY(!checker.isChecking());
        QVERIFY(!checker.hasUpdate());
        QVERIFY(!checker.currentVersion().isEmpty());
        QVERIFY(checker.latestVersion().isEmpty());
        QVERIFY(checker.errorMessage().isEmpty());

        checker.reset();
        QCOMPARE(checker.status(), UpdateChecker::Status::Idle);
    }

    void overrideVersionEnvVar()
    {
        qputenv("KTOMATO_OVERRIDE_VERSION", "0.9.0");
        UpdateChecker checker;
        QCOMPARE(checker.currentVersion(), QStringLiteral("0.9.0"));
        qunsetenv("KTOMATO_OVERRIDE_VERSION");
    }

    void releaseIsNewerThanItsPreReleases()
    {
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0"), QStringLiteral("1.1.0-rc1")), 1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v1.1.0-rc1"), QStringLiteral("1.1.0")), -1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0-rc1"), QStringLiteral("1.0.9")), 1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0-rc2"), QStringLiteral("1.1.0-rc10")), -1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0-beta"), QStringLiteral("1.1.0-rc1")), -1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0-rc.1"), QStringLiteral("1.1.0-rc.1.1")), -1);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0-rc1"), QStringLiteral("1.1.0-rc1")), 0);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("1.1.0+build5"), QStringLiteral("1.1.0")), 0);
    }

    void releaseUrlMustPointIntoTheProjectOnGitHub()
    {
        const QString fallback = QStringLiteral("https://github.com/MineraleYT/kTomato/releases");
        const QString good = QStringLiteral("https://github.com/MineraleYT/kTomato/releases/tag/v1.1.0");
        QCOMPARE(UpdateChecker::safeReleaseUrl(good), good);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QString()), fallback);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QStringLiteral("http://github.com/MineraleYT/kTomato/releases/tag/v1")), fallback);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QStringLiteral("https://evil.example/MineraleYT/kTomato/releases")), fallback);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QStringLiteral("https://github.com.evil.example/MineraleYT/kTomato/")), fallback);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QStringLiteral("https://user@github.com/MineraleYT/kTomato/releases")), fallback);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QStringLiteral("javascript:alert(1)")), fallback);
        QCOMPARE(UpdateChecker::safeReleaseUrl(QStringLiteral("https://github.com/someone/else/releases")), fallback);
    }

    void resetDuringACheckIgnoresTheReply()
    {
        UpdateChecker checker;
        checker.checkForUpdates();
        QCOMPARE(checker.status(), UpdateChecker::Status::Checking);
        checker.reset();
        QCOMPARE(checker.status(), UpdateChecker::Status::Idle);
        QTest::qWait(300); // the aborted reply finishes but must not change anything
        QCOMPARE(checker.status(), UpdateChecker::Status::Idle);
        QVERIFY(checker.errorMessage().isEmpty());
    }
};

QTEST_GUILESS_MAIN(UpdateCheckerTest)
#include "tst_updatechecker.moc"
