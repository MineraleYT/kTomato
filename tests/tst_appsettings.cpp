// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <memory>

#include "AppSettings.h"
#include "Autostart.h"

namespace
{
/// Autostart whose state the test controls; shared with the AppSettings under test.
struct FakeState {
    bool supported = true;
    bool enabled = false;
    bool failNext = false;
    int setCalls = 0;
    int repairCalls = 0;
};

class FakeAutostart final : public Autostart
{
public:
    explicit FakeAutostart(std::shared_ptr<FakeState> state)
        : m_state(std::move(state))
    {
    }

    bool isSupported() const override { return m_state->supported; }
    bool isEnabled() const override { return m_state->enabled; }
    bool setEnabled(bool enabled) override
    {
        ++m_state->setCalls;
        if (m_state->failNext) {
            m_state->failNext = false;
            return false;
        }
        m_state->enabled = enabled;
        return true;
    }
    void repair() override { ++m_state->repairCalls; }

private:
    std::shared_ptr<FakeState> m_state;
};
} // namespace

class AppSettingsTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;
    std::shared_ptr<FakeState> state;

    QString configPath() const { return dir.filePath(QStringLiteral("ktomatorc")); }

    std::unique_ptr<AppSettings> make(bool trayAvailable = true)
    {
        return std::make_unique<AppSettings>(KSharedConfig::openConfig(configPath(), KConfig::SimpleConfig),
                                             std::make_unique<FakeAutostart>(state), trayAvailable);
    }

    // Reads the file directly, not through the object under test.
    static QString missing() { return QStringLiteral("<missing>"); }

    QString stored(const char *key) const
    {
        KConfig config(configPath(), KConfig::SimpleConfig);
        return config.group(QStringLiteral("General")).readEntry(key, missing());
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("ktomato");
    }

    void init()
    {
        QFile::remove(configPath());
        state = std::make_shared<FakeState>();
    }

    void defaultsKeepTheProgramInTheTray()
    {
        const auto settings = make();
        QVERIFY(settings->showTrayIcon());
        QVERIFY(settings->closeToTray());
        QVERIFY(settings->startMinimized());
        QVERIFY(!settings->startAtLogin());
        QVERIFY(settings->keepRunningInTray());
        QVERIFY(settings->trayAvailable());
        QVERIFY(settings->autostartSupported());
        QCOMPARE(settings->trayBadgeStyle(), 0);
        QCOMPARE(settings->soundTheme(), QStringLiteral("alarm-clock-elapsed"));
        QCOMPARE(settings->dailyGoal(), 8);
        QVERIFY(settings->protectWeekendStreak());
        QCOMPARE(settings->soundVolume(), 80);
        QVERIFY(settings->preAlarmEnabled());
        QCOMPARE(settings->preAlarmSound(), QStringLiteral("dialog-information"));
        QCOMPARE(settings->focusSoundMode(), 0);
        QCOMPARE(settings->focusSoundVolume(), 30);
        QVERIFY(settings->keepScreenAwake());
    }

    void changesAreWrittenToTheConfigFile()
    {
        {
            const auto settings = make();
            settings->setShowTrayIcon(false);
            settings->setCloseToTray(false);
            settings->setStartMinimized(false);
            settings->setTrayBadgeStyle(1);
            settings->setSoundTheme(QStringLiteral("complete"));
            settings->setDailyGoal(12);
            settings->setProtectWeekendStreak(false);
        }
        QCOMPARE(stored("ShowTrayIcon"), QStringLiteral("false"));
        QCOMPARE(stored("CloseToTray"), QStringLiteral("false"));
        QCOMPARE(stored("StartMinimized"), QStringLiteral("false"));
        QCOMPARE(stored("TrayBadgeStyle"), QStringLiteral("1"));
        QCOMPARE(stored("SoundTheme"), QStringLiteral("complete"));
        QCOMPARE(stored("DailyGoal"), QStringLiteral("12"));
        QCOMPARE(stored("ProtectWeekendStreak"), QStringLiteral("false"));

        const auto reloaded = make();
        QVERIFY(!reloaded->showTrayIcon());
        QVERIFY(!reloaded->closeToTray());
        QVERIFY(!reloaded->startMinimized());
        QCOMPARE(reloaded->trayBadgeStyle(), 1);
        QCOMPARE(reloaded->soundTheme(), QStringLiteral("complete"));
        QCOMPARE(reloaded->dailyGoal(), 12);
        QVERIFY(!reloaded->protectWeekendStreak());
    }

    void focusAudioAndPowerSettingsPersistAndClamp()
    {
        {
            const auto settings = make();
            settings->setSoundVolume(130);
            settings->setPreAlarmEnabled(false);
            settings->setPreAlarmSound(QStringLiteral("bell"));
            settings->setFocusSoundMode(2);
            settings->setFocusSoundVolume(-10);
            settings->setKeepScreenAwake(false);
        }
        QCOMPARE(stored("SoundVolume"), QStringLiteral("100"));
        QCOMPARE(stored("PreAlarmEnabled"), QStringLiteral("false"));
        QCOMPARE(stored("PreAlarmSound"), QStringLiteral("bell"));
        QCOMPARE(stored("FocusSoundMode"), QStringLiteral("2"));
        QCOMPARE(stored("FocusSoundVolume"), QStringLiteral("0"));
        QCOMPARE(stored("KeepScreenAwake"), QStringLiteral("false"));

        const auto reloaded = make();
        QCOMPARE(reloaded->soundVolume(), 100);
        QVERIFY(!reloaded->preAlarmEnabled());
        QCOMPARE(reloaded->preAlarmSound(), QStringLiteral("bell"));
        QCOMPARE(reloaded->focusSoundMode(), 2);
        QCOMPARE(reloaded->focusSoundVolume(), 0);
        QVERIFY(!reloaded->keepScreenAwake());
    }

    void startAtLoginIsNotStoredInTheConfigFile()
    {
        const auto settings = make();
        settings->setStartAtLogin(true);
        settings->setShowTrayIcon(false); // forces a write
        QCOMPARE(stored("StartAtLogin"), missing());
        QCOMPARE(stored("startAtLogin"), missing());
    }

    void signalsFireOnlyOnRealChanges()
    {
        const auto settings = make();
        QSignalSpy tray(settings.get(), &AppSettings::showTrayIconChanged);
        QSignalSpy keep(settings.get(), &AppSettings::keepRunningInTrayChanged);
        QSignalSpy close(settings.get(), &AppSettings::closeToTrayChanged);
        QSignalSpy minimized(settings.get(), &AppSettings::startMinimizedChanged);
        QSignalSpy badge(settings.get(), &AppSettings::trayBadgeStyleChanged);
        QSignalSpy sound(settings.get(), &AppSettings::soundThemeChanged);
        QSignalSpy goal(settings.get(), &AppSettings::dailyGoalChanged);
        QSignalSpy streak(settings.get(), &AppSettings::protectWeekendStreakChanged);

        settings->setShowTrayIcon(true); // unchanged
        settings->setCloseToTray(true);
        settings->setStartMinimized(true);
        settings->setTrayBadgeStyle(0);
        settings->setSoundTheme(QStringLiteral("alarm-clock-elapsed"));
        settings->setDailyGoal(8);
        settings->setProtectWeekendStreak(true);
        QCOMPARE(tray.count(), 0);
        QCOMPARE(close.count(), 0);
        QCOMPARE(minimized.count(), 0);
        QCOMPARE(badge.count(), 0);
        QCOMPARE(sound.count(), 0);
        QCOMPARE(goal.count(), 0);
        QCOMPARE(streak.count(), 0);

        settings->setShowTrayIcon(false);
        QCOMPARE(tray.count(), 1);
        QCOMPARE(keep.count(), 1);
        settings->setCloseToTray(false);
        QCOMPARE(close.count(), 1);
        QCOMPARE(keep.count(), 2);
        settings->setStartMinimized(false);
        QCOMPARE(minimized.count(), 1);
        settings->setTrayBadgeStyle(2);
        QCOMPARE(badge.count(), 1);
        settings->setSoundTheme(QStringLiteral("bell"));
        QCOMPARE(sound.count(), 1);
        settings->setDailyGoal(10);
        QCOMPARE(goal.count(), 1);
        settings->setProtectWeekendStreak(false);
        QCOMPARE(streak.count(), 1);
    }

    void keepRunningInTrayNeedsTheIconTheOptionAndATray()
    {
        {
            const auto settings = make(true);
            QVERIFY(settings->keepRunningInTray());
            settings->setCloseToTray(false);
            QVERIFY(!settings->keepRunningInTray());
            settings->setCloseToTray(true);
            settings->setShowTrayIcon(false);
            QVERIFY(!settings->keepRunningInTray());
        }
        QFile::remove(configPath());
        const auto noTray = make(false); // nothing to hide into: closing must quit
        QVERIFY(!noTray->trayAvailable());
        QVERIFY(!noTray->keepRunningInTray());
    }

    void startHiddenNeedsEveryConditionAtOnce()
    {
        const auto settings = make(true);
        QVERIFY(!settings->startHidden()); // not launched with --background

        QSignalSpy spy(settings.get(), &AppSettings::startHiddenChanged);
        settings->setLaunchedInBackground(true);
        QVERIFY(settings->startHidden());
        QCOMPARE(spy.count(), 1);

        settings->setStartMinimized(false);
        QVERIFY(!settings->startHidden());
        settings->setStartMinimized(true);
        QVERIFY(settings->startHidden());
        settings->setShowTrayIcon(false);
        QVERIFY(!settings->startHidden()); // hidden with no icon would leave the program unreachable
    }

    void startHiddenIsOffWithoutATray()
    {
        const auto settings = make(false);
        settings->setLaunchedInBackground(true);
        QVERIFY(!settings->startHidden()); // never start invisible when there is no tray to find it in
    }

    void startAtLoginFollowsTheEntryOnDisk()
    {
        const auto settings = make();
        QVERIFY(!settings->startAtLogin());
        state->enabled = true; // another tool (System Settings) enabled it
        QVERIFY(settings->startAtLogin());
        state->enabled = false;
        QVERIFY(!settings->startAtLogin());
    }

    void settingStartAtLoginCreatesAndRemovesTheEntry()
    {
        const auto settings = make();
        settings->setStartAtLogin(true);
        QVERIFY(state->enabled);
        QVERIFY(settings->startAtLogin());
        settings->setStartAtLogin(false);
        QVERIFY(!state->enabled);
    }

    void settingTheCurrentValueDoesNotTouchTheEntry()
    {
        const auto settings = make();
        settings->setStartAtLogin(false);
        QCOMPARE(state->setCalls, 0);
    }

    void aRefusedChangeIsReportedAndTheStateStaysTrue()
    {
        const auto settings = make();
        QSignalSpy failed(settings.get(), &AppSettings::autostartFailed);
        QSignalSpy changed(settings.get(), &AppSettings::startAtLoginChanged);

        state->failNext = true;
        settings->setStartAtLogin(true);

        QCOMPARE(failed.count(), 1);
        QVERIFY(!failed.first().first().toString().isEmpty());
        QVERIFY(!settings->startAtLogin());  // the real state, not what was asked for
        QCOMPARE(changed.count(), 1);        // announced, so a checkbox shows the real state again
    }

    void aStaleEntryIsRepairedAtStartup()
    {
        make();
        QCOMPARE(state->repairCalls, 1);
    }

    void unsupportedPlatformsAreReported()
    {
        state->supported = false;
        const auto settings = make();
        QVERIFY(!settings->autostartSupported());
    }

    void firstRunCompletedDefaultsToFalseAndPersists()
    {
        {
            const auto settings = make();
            QVERIFY(!settings->firstRunCompleted());
            QSignalSpy spy(settings.get(), &AppSettings::firstRunCompletedChanged);
            settings->setFirstRunCompleted(true);
            QVERIFY(settings->firstRunCompleted());
            QCOMPARE(spy.count(), 1);
        }
        // Re-read from disk
        {
            const auto settings = make();
            QVERIFY(settings->firstRunCompleted());
        }
    }

    void languageSettingsAndEffectiveLanguage()
    {
        const auto settings = make();
        QCOMPARE(settings->language(), QStringLiteral("auto"));

        settings->setLanguage(QStringLiteral("es"));
        QCOMPARE(settings->language(), QStringLiteral("es"));
        QCOMPARE(settings->effectiveLanguage(), QStringLiteral("es"));

        settings->setLanguage(QStringLiteral("fr"));
        QCOMPARE(settings->language(), QStringLiteral("fr"));
        QCOMPARE(settings->effectiveLanguage(), QStringLiteral("fr"));

        settings->setLanguage(QStringLiteral("it"));
        QCOMPARE(settings->effectiveLanguage(), QStringLiteral("it"));

        settings->setLanguage(QStringLiteral("de"));
        QCOMPARE(settings->effectiveLanguage(), QStringLiteral("de"));

        settings->setLanguage(QStringLiteral("en"));
        QCOMPARE(settings->effectiveLanguage(), QStringLiteral("en"));
    }

    void autoPauseOnScreenLockDefaultsToTrueAndPersists()
    {
        {
            const auto settings = make();
            QVERIFY(settings->autoPauseOnScreenLock());
            QSignalSpy spy(settings.get(), &AppSettings::autoPauseOnScreenLockChanged);
            settings->setAutoPauseOnScreenLock(false);
            QVERIFY(!settings->autoPauseOnScreenLock());
            QCOMPARE(spy.count(), 1);
        }
        // Re-read from disk
        {
            const auto settings = make();
            QVERIFY(!settings->autoPauseOnScreenLock());
        }
    }

    void promptTaskNoteDefaultsToTrueAndPersists()
    {
        {
            const auto settings = make();
            QVERIFY(settings->promptTaskNote());
            QSignalSpy spy(settings.get(), &AppSettings::promptTaskNoteChanged);
            settings->setPromptTaskNote(false);
            QVERIFY(!settings->promptTaskNote());
            QCOMPARE(spy.count(), 1);
        }
        // Re-read from disk
        {
            const auto settings = make();
            QVERIFY(!settings->promptTaskNote());
        }
    }

    void invalidStoredValuesAreClamped()
    {
        {
            KConfig config(configPath(), KConfig::SimpleConfig);
            KConfigGroup group = config.group(QStringLiteral("General"));
            group.writeEntry("TrayBadgeStyle", 9);
            group.writeEntry("FocusSoundMode", 7);
            group.writeEntry("DailyGoal", 500);
            group.writeEntry("ColorScheme", QStringLiteral("purple"));
            config.sync();
        }
        {
            const auto settings = make();
            QCOMPARE(settings->trayBadgeStyle(), 2);
            QCOMPARE(settings->focusSoundMode(), 3);
            QCOMPARE(settings->dailyGoal(), AppSettings::MaxDailyGoal);
            QCOMPARE(settings->colorScheme(), QStringLiteral("dark"));
        } // releases the shared config, so the next one rereads the file

        {
            KConfig config(configPath(), KConfig::SimpleConfig);
            KConfigGroup group = config.group(QStringLiteral("General"));
            group.writeEntry("TrayBadgeStyle", -1);
            group.writeEntry("FocusSoundMode", -4);
            group.writeEntry("DailyGoal", -3);
            config.sync();
        }
        const auto reloaded = make();
        QCOMPARE(reloaded->trayBadgeStyle(), 0);
        QCOMPARE(reloaded->focusSoundMode(), 0);
        QCOMPARE(reloaded->dailyGoal(), 0);
    }

    void dailyGoalZeroDisablesTheGoal()
    {
        const auto settings = make();
        settings->setDailyGoal(0);
        QCOMPARE(settings->dailyGoal(), 0);
        QCOMPARE(stored("DailyGoal"), QStringLiteral("0"));
        settings->setDailyGoal(-5);
        QCOMPARE(settings->dailyGoal(), 0);
        settings->setDailyGoal(1000);
        QCOMPARE(settings->dailyGoal(), AppSettings::MaxDailyGoal);
    }

    void colorSchemeDefaultsToDarkAndPersists()
    {
        {
            const auto settings = make();
            QCOMPARE(settings->colorScheme(), QStringLiteral("dark"));
            QSignalSpy spy(settings.get(), &AppSettings::colorSchemeChanged);
            settings->setColorScheme(QStringLiteral("light"));
            QCOMPARE(settings->colorScheme(), QStringLiteral("light"));
            QCOMPARE(spy.count(), 1);
            settings->setColorScheme(QStringLiteral("bogus")); // falls back to dark
            QCOMPARE(settings->colorScheme(), QStringLiteral("dark"));
            settings->setColorScheme(QStringLiteral("system"));
            QCOMPARE(spy.count(), 3);
        }
        QCOMPARE(stored("ColorScheme"), QStringLiteral("system"));
        QCOMPARE(make()->colorScheme(), QStringLiteral("system"));
    }

    void trayAvailabilityChangesUpdateDependentProperties()
    {
        const auto settings = make(false);
        QVERIFY(!settings->keepRunningInTray());
        QSignalSpy tray(settings.get(), &AppSettings::trayAvailableChanged);
        QSignalSpy keep(settings.get(), &AppSettings::keepRunningInTrayChanged);
        QSignalSpy hidden(settings.get(), &AppSettings::startHiddenChanged);

        settings->setTrayAvailable(true);
        QVERIFY(settings->trayAvailable());
        QVERIFY(settings->keepRunningInTray());
        QCOMPARE(tray.count(), 1);
        QCOMPARE(keep.count(), 1);
        QCOMPARE(hidden.count(), 1);

        settings->setTrayAvailable(true); // no change, no signal
        QCOMPARE(tray.count(), 1);

        settings->setTrayAvailable(false);
        QVERIFY(!settings->keepRunningInTray());
        QCOMPARE(tray.count(), 2);
    }

    void explicitLanguageAlsoSetsTheDefaultLocale()
    {
        const auto settings = make();
        settings->setLanguage(QStringLiteral("de"));
        QCOMPARE(QLocale().language(), QLocale::German);
        settings->setLanguage(QStringLiteral("auto"));
        QCOMPARE(QLocale(), QLocale::system());
    }

    void storedLanguageIsAppliedWithoutAnInstance()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/ktomatorc");
        {
            KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("ktomatorc"));
            config->group(QStringLiteral("General")).writeEntry("Language", QStringLiteral("it"));
            config->sync();
        }
        AppSettings::applyStoredLanguage();
        QCOMPARE(QLocale().language(), QLocale::Italian);

        QFile::remove(path);
        KLocalizedString::clearLanguages();
        QLocale::setDefault(QLocale::system());
        QStandardPaths::setTestModeEnabled(false);
    }
};

QTEST_GUILESS_MAIN(AppSettingsTest)
#include "tst_appsettings.moc"
