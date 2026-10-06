// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppSettings.h"

#include "Autostart.h"
#include "SystemTray.h"

#include <QDBusConnection>
#include <QDebug>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <KColorSchemeManager>
#include <KConfigGroup>
#include <KLocalizedString>
#include <algorithm>


namespace
{
const char kGroup[] = "General";
const char kShowTrayIcon[] = "ShowTrayIcon";
const char kCloseToTray[] = "CloseToTray";
const char kStartMinimized[] = "StartMinimized";
const char kTrayBadgeStyle[] = "TrayBadgeStyle";
const char kSoundTheme[] = "SoundTheme";
const char kSoundVolume[] = "SoundVolume";
const char kPreAlarmEnabled[] = "PreAlarmEnabled";
const char kPreAlarmSound[] = "PreAlarmSound";
const char kFocusSoundMode[] = "FocusSoundMode";
const char kFocusSoundVolume[] = "FocusSoundVolume";
const char kKeepScreenAwake[] = "KeepScreenAwake";
const char kAutoPauseOnScreenLock[] = "AutoPauseOnScreenLock";
const char kDailyGoal[] = "DailyGoal";
const char kProtectWeekendStreak[] = "ProtectWeekendStreak";
const char kFirstRunCompleted[] = "FirstRunCompleted";
const char kPromptTaskNote[] = "PromptTaskNote";
const char kLanguage[] = "Language";
const char kColorScheme[] = "ColorScheme";

QString validColorScheme(const QString &scheme)
{
    const QString s = scheme.trimmed().toLower();
    if (s == QLatin1String("light") || s == QLatin1String("system")) {
        return s;
    }
    return QStringLiteral("dark");
}

/// A sound file for an event sound name, looked up in the XDG data dirs (so it also works
/// inside Flatpak), then in the host's /usr/share. An absolute path is used as is.
QString findSoundFile(const QString &soundName)
{
    if (soundName.startsWith(QLatin1Char('/'))) {
        return QFile::exists(soundName) ? soundName : QString();
    }
    for (const char *theme : {"ocean", "freedesktop"}) {
        for (const char *ext : {"oga", "ogg", "wav"}) {
            const QString relative = QStringLiteral("sounds/%1/stereo/%2.%3").arg(QLatin1String(theme), soundName, QLatin1String(ext));
            QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relative);
            if (file.isEmpty()) {
                for (const QString &prefix : {QStringLiteral("/usr/share/"), QStringLiteral("/run/host/usr/share/")}) {
                    if (QFile::exists(prefix + relative)) {
                        file = prefix + relative;
                        break;
                    }
                }
            }
            if (!file.isEmpty()) {
                return file;
            }
        }
    }
    return {};
}
} // namespace

AppSettings::AppSettings(QObject *parent)
    : AppSettings(KSharedConfig::openConfig(QStringLiteral("ktomatorc")), createAutostart(), systemTrayAvailable(), parent)
{
    watchTray();
}

AppSettings::AppSettings(KSharedConfig::Ptr config, std::unique_ptr<Autostart> autostart, bool trayAvailable, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_autostart(std::move(autostart))
    , m_trayAvailable(trayAvailable)
{
    const KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    m_showTrayIcon = group.readEntry(kShowTrayIcon, true);
    m_closeToTray = group.readEntry(kCloseToTray, true);
    m_startMinimized = group.readEntry(kStartMinimized, true);
    m_trayBadgeStyle = std::clamp(group.readEntry(kTrayBadgeStyle, 0), 0, 2);
    m_soundTheme = group.readEntry(kSoundTheme, QStringLiteral("alarm-clock-elapsed"));
    m_soundVolume = std::clamp(group.readEntry(kSoundVolume, 80), 0, 100);
    m_preAlarmEnabled = group.readEntry(kPreAlarmEnabled, true);
    m_preAlarmSound = group.readEntry(kPreAlarmSound, QStringLiteral("dialog-information"));
    m_focusSoundMode = std::clamp(group.readEntry(kFocusSoundMode, 0), 0, 3);
    m_focusSoundVolume = std::clamp(group.readEntry(kFocusSoundVolume, 30), 0, 100);
    m_keepScreenAwake = group.readEntry(kKeepScreenAwake, true);
    m_autoPauseOnScreenLock = group.readEntry(kAutoPauseOnScreenLock, true);
    m_dailyGoal = std::clamp(group.readEntry(kDailyGoal, 8), 0, MaxDailyGoal);
    m_protectWeekendStreak = group.readEntry(kProtectWeekendStreak, true);
    m_firstRunCompleted = group.readEntry(kFirstRunCompleted, false);
    m_promptTaskNote = group.readEntry(kPromptTaskNote, true);
    m_language = group.readEntry(kLanguage, QStringLiteral("auto"));
    m_colorScheme = validColorScheme(group.readEntry(kColorScheme, QStringLiteral("dark")));
    applyLanguage();
    applyColorScheme();

    // The program may have moved since the entry was written (new install prefix).
    if (m_autostart) {
        m_autostart->repair();
    }
}

AppSettings::~AppSettings() = default;

void AppSettings::store(const char *key, bool value)
{
    KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    group.writeEntry(key, value);
    m_config->sync();
}

void AppSettings::storeInt(const char *key, int value)
{
    KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    group.writeEntry(key, value);
    m_config->sync();
}

void AppSettings::storeString(const char *key, const QString &value)
{
    KConfigGroup group = m_config->group(QString::fromLatin1(kGroup));
    group.writeEntry(key, value);
    m_config->sync();
}

void AppSettings::watchTray()
{
    // The tray host can appear after us (login) or restart (plasmashell crash).
    auto *watcher = new QDBusServiceWatcher(QStringLiteral("org.kde.StatusNotifierWatcher"),
                                            QDBusConnection::sessionBus(),
                                            QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                            this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        setTrayAvailable(true);
    });
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        setTrayAvailable(false);
    });
}

void AppSettings::setTrayAvailable(bool available)
{
    if (m_trayAvailable == available) {
        return;
    }
    m_trayAvailable = available;
    Q_EMIT trayAvailableChanged();
    Q_EMIT keepRunningInTrayChanged();
    Q_EMIT startHiddenChanged();
}

bool AppSettings::autostartSupported() const
{
    return m_autostart && m_autostart->isSupported();
}

void AppSettings::setShowTrayIcon(bool show)
{
    if (m_showTrayIcon == show) {
        return;
    }
    m_showTrayIcon = show;
    store(kShowTrayIcon, show);
    Q_EMIT showTrayIconChanged();
    Q_EMIT keepRunningInTrayChanged();
    Q_EMIT startHiddenChanged();
}

void AppSettings::setCloseToTray(bool close)
{
    if (m_closeToTray == close) {
        return;
    }
    m_closeToTray = close;
    store(kCloseToTray, close);
    Q_EMIT closeToTrayChanged();
    Q_EMIT keepRunningInTrayChanged();
}

bool AppSettings::startAtLogin() const
{
    return m_autostart && m_autostart->isEnabled();
}

void AppSettings::setStartAtLogin(bool enabled)
{
    if (!m_autostart) {
        return;
    }
    if (enabled != startAtLogin() && !m_autostart->setEnabled(enabled)) {
        Q_EMIT autostartFailed(enabled ? i18n("Could not set up starting kTomato at login.")
                                       : i18n("Could not remove the entry that starts kTomato at login."));
    }
    // Always announced: after a failure this makes views show the real state again.
    Q_EMIT startAtLoginChanged();
}

void AppSettings::setStartMinimized(bool minimized)
{
    if (m_startMinimized == minimized) {
        return;
    }
    m_startMinimized = minimized;
    store(kStartMinimized, minimized);
    Q_EMIT startMinimizedChanged();
    Q_EMIT startHiddenChanged();
}

void AppSettings::setTrayBadgeStyle(int style)
{
    style = std::clamp(style, 0, 2);
    if (m_trayBadgeStyle == style) {
        return;
    }
    m_trayBadgeStyle = style;
    storeInt(kTrayBadgeStyle, style);
    Q_EMIT trayBadgeStyleChanged();
}

void AppSettings::setSoundTheme(const QString &theme)
{
    if (m_soundTheme == theme) {
        return;
    }
    m_soundTheme = theme;
    storeString(kSoundTheme, theme);
    Q_EMIT soundThemeChanged();
}

void AppSettings::setSoundVolume(int volume)
{
    volume = std::clamp(volume, 0, 100);
    if (m_soundVolume == volume) {
        return;
    }
    m_soundVolume = volume;
    storeInt(kSoundVolume, volume);
    Q_EMIT soundVolumeChanged();
}

void AppSettings::setPreAlarmEnabled(bool enabled)
{
    if (m_preAlarmEnabled == enabled) {
        return;
    }
    m_preAlarmEnabled = enabled;
    store(kPreAlarmEnabled, enabled);
    Q_EMIT preAlarmEnabledChanged();
}

void AppSettings::setPreAlarmSound(const QString &sound)
{
    if (m_preAlarmSound == sound) {
        return;
    }
    m_preAlarmSound = sound;
    storeString(kPreAlarmSound, sound);
    Q_EMIT preAlarmSoundChanged();
}

void AppSettings::setFocusSoundMode(int mode)
{
    mode = std::clamp(mode, 0, 3);
    if (m_focusSoundMode == mode) {
        return;
    }
    m_focusSoundMode = mode;
    storeInt(kFocusSoundMode, mode);
    Q_EMIT focusSoundModeChanged();
}

void AppSettings::setFocusSoundVolume(int volume)
{
    volume = std::clamp(volume, 0, 100);
    if (m_focusSoundVolume == volume) {
        return;
    }
    m_focusSoundVolume = volume;
    storeInt(kFocusSoundVolume, volume);
    Q_EMIT focusSoundVolumeChanged();
}

void AppSettings::setKeepScreenAwake(bool awake)
{
    if (m_keepScreenAwake == awake) {
        return;
    }
    m_keepScreenAwake = awake;
    store(kKeepScreenAwake, awake);
    Q_EMIT keepScreenAwakeChanged();
}

void AppSettings::setAutoPauseOnScreenLock(bool pause)
{
    if (m_autoPauseOnScreenLock == pause) {
        return;
    }
    m_autoPauseOnScreenLock = pause;
    store(kAutoPauseOnScreenLock, pause);
    Q_EMIT autoPauseOnScreenLockChanged();
}

void AppSettings::setDailyGoal(int goal)
{
    goal = std::clamp(goal, 0, MaxDailyGoal);
    if (m_dailyGoal == goal) {
        return;
    }
    m_dailyGoal = goal;
    storeInt(kDailyGoal, goal);
    Q_EMIT dailyGoalChanged();
}

void AppSettings::setProtectWeekendStreak(bool protect)
{
    if (m_protectWeekendStreak == protect) {
        return;
    }
    m_protectWeekendStreak = protect;
    store(kProtectWeekendStreak, protect);
    Q_EMIT protectWeekendStreakChanged();
}

void AppSettings::setFirstRunCompleted(bool completed)
{
    if (m_firstRunCompleted == completed) {
        return;
    }
    m_firstRunCompleted = completed;
    store(kFirstRunCompleted, completed);
    Q_EMIT firstRunCompletedChanged();
}

void AppSettings::setPromptTaskNote(bool prompt)
{
    if (m_promptTaskNote == prompt) {
        return;
    }
    m_promptTaskNote = prompt;
    store(kPromptTaskNote, prompt);
    Q_EMIT promptTaskNoteChanged();
}

QString AppSettings::resolveLanguage(const QString &setting)
{
    static const QStringList supported = {QStringLiteral("en"), QStringLiteral("it"), QStringLiteral("de"),
                                          QStringLiteral("es"), QStringLiteral("fr")};
    if (supported.contains(setting)) {
        return setting;
    }
    const QString sysLang = QLocale::system().name().section(QLatin1Char('_'), 0, 0).toLower();
    return supported.contains(sysLang) ? sysLang : QStringLiteral("en");
}

QString AppSettings::effectiveLanguage() const
{
    return resolveLanguage(m_language);
}

void AppSettings::applyLanguageSetting(const QString &setting)
{
    const QString eff = resolveLanguage(setting);
    KLocalizedString::setLanguages({eff});
    // An explicit choice also changes number and date formats; "auto" keeps the user's regional formats.
    const bool explicitChoice = eff == setting;
    QLocale::setDefault(explicitChoice ? QLocale(eff) : QLocale::system());
}

void AppSettings::applyLanguage()
{
    applyLanguageSetting(m_language);
}

void AppSettings::applyStoredLanguage()
{
    const KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("ktomatorc"))->group(QString::fromLatin1(kGroup));
    applyLanguageSetting(group.readEntry(kLanguage, QStringLiteral("auto")).trimmed().toLower());
}

void AppSettings::setLanguage(const QString &language)
{
    const QString normalized = language.trimmed().toLower();
    if (m_language == normalized) {
        return;
    }
    m_language = normalized.isEmpty() ? QStringLiteral("auto") : normalized;
    storeString(kLanguage, m_language);
    applyLanguage();
    Q_EMIT languageChanged();
}

void AppSettings::setColorScheme(const QString &scheme)
{
    const QString valid = validColorScheme(scheme);
    if (m_colorScheme == valid) {
        return;
    }
    m_colorScheme = valid;
    storeString(kColorScheme, valid);
    applyColorScheme();
    Q_EMIT colorSchemeChanged();
}

void AppSettings::applyColorScheme()
{
    // The manager installs a palette, so it needs a GUI application.
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        return;
    }
    QString id; // empty: the system scheme
    if (m_colorScheme == QLatin1String("dark")) {
        id = QStringLiteral("BreezeDark");
    } else if (m_colorScheme == QLatin1String("light")) {
        id = QStringLiteral("BreezeLight");
    }
    KColorSchemeManager::instance()->activateSchemeId(id);
}

void AppSettings::playSoundPreview(const QString &soundName, int volume)
{
    const int vol = (volume >= 0) ? std::clamp(volume, 0, 100) : m_soundVolume;
    const QString file = findSoundFile(soundName);
    if (file.isEmpty()) {
        // Not a file we can find: let libcanberra resolve the event sound name.
        if (QStandardPaths::findExecutable(QStringLiteral("canberra-gtk-play")).isEmpty()
            || !QProcess::startDetached(QStringLiteral("canberra-gtk-play"), {QStringLiteral("-i"), soundName})) {
            qWarning() << "Sound" << soundName << "not found and canberra-gtk-play is unavailable";
        }
        return;
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("pw-play")).isEmpty()) {
        const double volFactor = vol / 100.0;
        if (QProcess::startDetached(QStringLiteral("pw-play"), {QStringLiteral("--volume"), QString::number(volFactor, 'f', 2), file})) {
            return;
        }
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("paplay")).isEmpty()) {
        // paplay's volume is linear, 65536 = 100 %.
        const int paVolume = vol * 65536 / 100;
        if (QProcess::startDetached(QStringLiteral("paplay"), {QStringLiteral("--volume=%1").arg(paVolume), file})) {
            return;
        }
    }
    qWarning() << "Cannot play" << file << "- neither pw-play nor paplay could be started";
}

void AppSettings::openShortcutsSettings()
{
    if (!QProcess::startDetached(QStringLiteral("systemsettings"), {QStringLiteral("kcm_keys")})) {
        qWarning() << "Failed to launch systemsettings kcm_keys";
    }
}

bool AppSettings::keepRunningInTray() const
{
    return m_showTrayIcon && m_closeToTray && m_trayAvailable;
}

bool AppSettings::startHidden() const
{
    return m_launchedInBackground && m_startMinimized && m_showTrayIcon && m_trayAvailable;
}

void AppSettings::setLaunchedInBackground(bool background)
{
    m_launchedInBackground = background;
    Q_EMIT startHiddenChanged();
}
