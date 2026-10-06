// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresetModel.h"

#include <KLocalizedString>

#include <QSet>
#include <QUuid>

#include <algorithm>
#include <optional>

const QString PresetModel::DefaultUuid = QStringLiteral("default");

namespace
{
constexpr int kMaxWorkSeconds = 6 * 3600;
constexpr int kMaxBreakSeconds = 2 * 3600;
constexpr int kMaxCycles = 99;

int clampedInt(const QVariant &value, int low, int high, int fallback)
{
    bool ok = false;
    const int n = value.toInt(&ok);
    return ok ? std::clamp(n, low, high) : fallback;
}

/// "25 min", "30 s", or "1 min 30 s" for durations that are not whole minutes.
QString durationLabel(int seconds)
{
    seconds = std::max(0, seconds);
    const int minutes = seconds / 60;
    const int rest = seconds % 60;
    if (rest == 0) {
        return i18ncp("duration in minutes", "%1 min", "%1 min", minutes);
    }
    if (minutes == 0) {
        return i18ncp("duration in seconds", "%1 s", "%1 s", rest);
    }
    return i18nc("duration in minutes and seconds", "%1 min %2 s", minutes, rest);
}

/// The registry entry of an option, or nothing for the legacy AutoStart key and unknown keys.
std::optional<OptionDescriptor> descriptorFor(const QString &key)
{
    const auto all = PresetSettings::descriptors();
    for (const OptionDescriptor &d : all) {
        if (d.key == key) {
            return d;
        }
    }
    return std::nullopt;
}

/// The value as stored for this option: ints clamped to the option's own range, others as bool.
QVariant coercedOption(const OptionDescriptor &desc, const QVariant &value)
{
    if (desc.type == QLatin1String("int")) {
        return clampedInt(value, desc.minimum, desc.maximum, desc.defaultValue.toInt());
    }
    return value.toBool();
}

/// The built-in preset was once called "Default", stored in whatever language was active then.
bool isLegacyDefaultName(const QString &name)
{
    if (name.isEmpty() || name == QLatin1String("Default")) {
        return true;
    }
    const auto languages = KLocalizedString::availableApplicationTranslations();
    for (const QString &language : languages) {
        if (name == ki18n("Default").toString(QStringList{language})) {
            return true;
        }
    }
    return false;
}
} // namespace

PresetModel::PresetModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_currentUuid(DefaultUuid)
{
    m_presets.append(makeDefaultPreset());
}

TimerPreset PresetModel::makeDefaultPreset()
{
    TimerPreset preset;
    preset.uuid = DefaultUuid;
    preset.name = i18n("Pomodoro");
    preset.category = i18n("Work");
    preset.builtin = true;
    return preset;
}

QList<TimerPreset> PresetModel::standardPresets()
{
    TimerPreset pomodoro = makeDefaultPreset();
    pomodoro.options.insert(QStringLiteral("iconName"), QStringLiteral("chronometer"));

    TimerPreset deepWork;
    deepWork.uuid = QStringLiteral("deep-work");
    deepWork.name = i18n("Deep Work");
    deepWork.category = i18n("Focus");
    deepWork.workSeconds = 50 * 60;
    deepWork.shortBreakSeconds = 10 * 60;
    deepWork.longBreakSeconds = 30 * 60;
    deepWork.cyclesBeforeLong = 2;
    deepWork.builtin = false;
    deepWork.options.insert(QStringLiteral("iconName"), QStringLiteral("flash-symbolic"));

    TimerPreset study;
    study.uuid = QStringLiteral("study");
    study.name = i18n("Study");
    study.category = i18n("Study");
    study.workSeconds = 45 * 60;
    study.shortBreakSeconds = 15 * 60;
    study.longBreakSeconds = 20 * 60;
    study.cyclesBeforeLong = 3;
    study.builtin = false;
    study.options.insert(QStringLiteral("iconName"), QStringLiteral("view-readermode-symbolic"));

    TimerPreset quickSprint;
    quickSprint.uuid = QStringLiteral("quick-sprint");
    quickSprint.name = i18n("Quick Sprint");
    quickSprint.category = i18n("Tasks");
    quickSprint.workSeconds = 15 * 60;
    quickSprint.shortBreakSeconds = 3 * 60;
    quickSprint.longBreakSeconds = 10 * 60;
    quickSprint.cyclesBeforeLong = 4;
    quickSprint.builtin = false;
    quickSprint.options.insert(QStringLiteral("iconName"), QStringLiteral("speedometer-symbolic"));

    return {pomodoro, deepWork, study, quickSprint};
}

void PresetModel::addStandardPresets()
{
    const QList<TimerPreset> standards = standardPresets();
    bool addedAny = false;
    for (const auto &sp : standards) {
        const int idx = indexOf(sp.uuid);
        if (idx < 0) {
            const int row = count();
            beginInsertRows({}, row, row);
            m_presets.append(sp);
            endInsertRows();
            addedAny = true;
        } else if (sp.uuid == DefaultUuid) {
            bool changed = false;
            if (isLegacyDefaultName(m_presets[idx].name)) {
                m_presets[idx].name = sp.name;
                changed = true;
            }
            if (m_presets[idx].category.isEmpty()) {
                m_presets[idx].category = sp.category;
                changed = true;
            }
            if (changed) {
                Q_EMIT dataChanged(index(idx), index(idx));
                Q_EMIT presetChanged(DefaultUuid);
                if (m_currentUuid == DefaultUuid) {
                    Q_EMIT currentChanged();
                }
                addedAny = true;
            }
        }
    }
    if (addedAny) {
        Q_EMIT countChanged();
        Q_EMIT modified();
    }
}

QVariantList PresetModel::availableIcons() const
{
    return {
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("chronometer")}, {QStringLiteral("label"), i18n("Timer / Pomodoro")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("io.github.mineraleyt.ktomato")}, {QStringLiteral("label"), i18n("Tomato")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("flash-symbolic")}, {QStringLiteral("label"), i18n("Focus / Energy")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("view-readermode-symbolic")}, {QStringLiteral("label"), i18n("Study / Reading")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("speedometer-symbolic")}, {QStringLiteral("label"), i18n("Sprint / Speed")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("document-edit-symbolic")}, {QStringLiteral("label"), i18n("Writing / Notes")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("system-run-symbolic")}, {QStringLiteral("label"), i18n("Coding / Execution")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("view-pim-tasks-symbolic")}, {QStringLiteral("label"), i18n("Tasks / Checklist")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("bookmarks-symbolic")}, {QStringLiteral("label"), i18n("Research / Books")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("flag-symbolic")}, {QStringLiteral("label"), i18n("Goal / Milestone")}},
        QVariantMap{{QStringLiteral("iconName"), QStringLiteral("favorites-symbolic")}, {QStringLiteral("label"), i18n("Favorite")}},
    };
}

void PresetModel::applyFields(TimerPreset &preset, const QVariantMap &fields)
{
    if (fields.contains(QStringLiteral("name"))) {
        const QString name = fields.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            preset.name = name;
        }
    }
    if (fields.contains(QStringLiteral("category"))) {
        preset.category = fields.value(QStringLiteral("category")).toString().trimmed();
    }
    if (fields.contains(QStringLiteral("iconName"))) {
        const QString icon = fields.value(QStringLiteral("iconName")).toString().trimmed();
        if (!icon.isEmpty()) {
            preset.options.insert(QStringLiteral("iconName"), icon);
        } else {
            preset.options.remove(QStringLiteral("iconName"));
        }
    }
    preset.workSeconds = clampedInt(fields.value(QStringLiteral("workSeconds")), 1, kMaxWorkSeconds, preset.workSeconds);
    preset.shortBreakSeconds = clampedInt(fields.value(QStringLiteral("shortBreakSeconds")), 0, kMaxBreakSeconds, preset.shortBreakSeconds);
    preset.longBreakSeconds = clampedInt(fields.value(QStringLiteral("longBreakSeconds")), 0, kMaxBreakSeconds, preset.longBreakSeconds);
    preset.cyclesBeforeLong = clampedInt(fields.value(QStringLiteral("cyclesBeforeLong")), 0, kMaxCycles, preset.cyclesBeforeLong);

    for (const auto &desc : PresetSettings::descriptors()) {
        if (fields.contains(desc.key)) {
            const QVariant value = coercedOption(desc, fields.value(desc.key));
            preset.options.insert(desc.key, value);
            if (desc.key == PresetOption::AutoStartMode) {
                preset.options.insert(PresetOption::AutoStart, value.toInt() == 2);
            }
        }
    }
}

QString PresetModel::summaryFor(const TimerPreset &preset)
{
    return i18nc("%1 work duration, %2 break duration", "%1 work, %2 break",
                 durationLabel(preset.workSeconds), durationLabel(preset.shortBreakSeconds));
}

QString PresetModel::detailedSummaryFor(const TimerPreset &preset)
{
    if (preset.cyclesBeforeLong > 0 && preset.longBreakSeconds > 0) {
        return i18nc("%1 work duration, %2 break duration, %3 long break duration, %4 number of cycles",
                     "%1 work • %2 break • %3 long break (%4)",
                     durationLabel(preset.workSeconds),
                     durationLabel(preset.shortBreakSeconds),
                     durationLabel(preset.longBreakSeconds),
                     i18np("%1 cycle", "%1 cycles", preset.cyclesBeforeLong));
    }
    return i18nc("%1 work duration, %2 break duration",
                 "%1 work • %2 break",
                 durationLabel(preset.workSeconds),
                 durationLabel(preset.shortBreakSeconds));
}

QString PresetModel::iconNameFor(const TimerPreset &preset)
{
    const QString custom = preset.options.value(QStringLiteral("iconName")).toString().trimmed();
    if (!custom.isEmpty()) {
        return custom;
    }

    const QString id = preset.uuid;
    const QString cat = preset.category.toLower();
    const QString name = preset.name.toLower();

    if (id == QStringLiteral("deep-work") || cat.contains(QStringLiteral("focus")) || name.contains(QStringLiteral("deep"))) {
        return QStringLiteral("flash-symbolic");
    }
    if (id == QStringLiteral("study") || cat.contains(QStringLiteral("study")) || cat.contains(QStringLiteral("edu")) || cat.contains(QStringLiteral("learn")) || name.contains(QStringLiteral("study"))) {
        return QStringLiteral("view-readermode-symbolic");
    }
    if (id == QStringLiteral("quick-sprint") || cat.contains(QStringLiteral("sprint")) || cat.contains(QStringLiteral("task")) || cat.contains(QStringLiteral("quick")) || name.contains(QStringLiteral("sprint"))) {
        return QStringLiteral("speedometer-symbolic");
    }
    return QStringLiteral("chronometer");
}

// --- Model interface -------------------------------------------------------

int PresetModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant PresetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) {
        return {};
    }
    const TimerPreset &p = m_presets.at(index.row());
    switch (role) {
    case UuidRole:
        return p.uuid;
    case Qt::DisplayRole:
    case NameRole:
        return p.name;
    case CategoryRole:
        return p.category;
    case WorkSecondsRole:
        return p.workSeconds;
    case ShortBreakSecondsRole:
        return p.shortBreakSeconds;
    case LongBreakSecondsRole:
        return p.longBreakSeconds;
    case CyclesBeforeLongRole:
        return p.cyclesBeforeLong;
    case BuiltinRole:
        return p.builtin;
    case SummaryRole:
        return summaryFor(p);
    case DetailedSummaryRole:
        return detailedSummaryFor(p);
    case IconNameRole:
        return iconNameFor(p);
    case WorkMinutesRole:
        return std::max(1, p.workSeconds / 60);
    }
    return {};
}

QHash<int, QByteArray> PresetModel::roleNames() const
{
    return {
        {UuidRole, "uuid"},
        {NameRole, "name"},
        {CategoryRole, "category"},
        {WorkSecondsRole, "workSeconds"},
        {ShortBreakSecondsRole, "shortBreakSeconds"},
        {LongBreakSecondsRole, "longBreakSeconds"},
        {CyclesBeforeLongRole, "cyclesBeforeLong"},
        {BuiltinRole, "builtin"},
        {SummaryRole, "summary"},
        {DetailedSummaryRole, "detailedSummary"},
        {IconNameRole, "iconName"},
        {WorkMinutesRole, "workMinutes"},
    };
}

void PresetModel::setPresets(const QList<TimerPreset> &presets, const QString &currentUuid)
{
    QList<TimerPreset> clean;
    QSet<QString> seen;
    for (TimerPreset preset : presets) {
        if (preset.uuid.isEmpty() || seen.contains(preset.uuid)) {
            continue;
        }
        seen.insert(preset.uuid);
        preset.builtin = (preset.uuid == DefaultUuid);
        clean.append(preset);
    }
    if (!seen.contains(DefaultUuid)) {
        clean.prepend(makeDefaultPreset());
    }

    beginResetModel();
    m_presets = clean;
    m_currentUuid = seen.contains(currentUuid) ? currentUuid : DefaultUuid;
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT currentChanged();
}

// --- Selection -------------------------------------------------------------

void PresetModel::setCurrentUuid(const QString &uuid)
{
    if (uuid == m_currentUuid || indexOf(uuid) < 0) {
        return;
    }
    m_currentUuid = uuid;
    Q_EMIT currentChanged();
}

TimerPreset PresetModel::currentPreset() const
{
    const int row = currentIndex();
    return row >= 0 ? m_presets.at(row) : makeDefaultPreset();
}

QString PresetModel::currentName() const
{
    return currentPreset().name;
}

QString PresetModel::currentCategory() const
{
    return currentPreset().category;
}

QString PresetModel::currentSummary() const
{
    return summaryFor(currentPreset());
}

QString PresetModel::currentDetailedSummary() const
{
    return detailedSummaryFor(currentPreset());
}

QString PresetModel::currentIconName() const
{
    return iconNameFor(currentPreset());
}

int PresetModel::currentWorkMinutes() const
{
    return std::max(1, currentPreset().workSeconds / 60);
}

int PresetModel::indexOf(const QString &uuid) const
{
    for (int i = 0; i < count(); ++i) {
        if (m_presets.at(i).uuid == uuid) {
            return i;
        }
    }
    return -1;
}

QVariantMap PresetModel::get(const QString &uuid) const
{
    const int row = indexOf(uuid);
    if (row < 0) {
        return {};
    }
    const TimerPreset &p = m_presets.at(row);

    QVariantMap options = PresetSettings::defaults();
    if (p.options.contains(PresetOption::AutoStart) && !p.options.contains(PresetOption::AutoStartMode)) {
        options.insert(PresetOption::AutoStartMode, p.options.value(PresetOption::AutoStart).toBool() ? 2 : 0);
    }
    options.insert(p.options);

    return {
        {QStringLiteral("uuid"), p.uuid},
        {QStringLiteral("name"), p.name},
        {QStringLiteral("category"), p.category},
        {QStringLiteral("workSeconds"), p.workSeconds},
        {QStringLiteral("shortBreakSeconds"), p.shortBreakSeconds},
        {QStringLiteral("longBreakSeconds"), p.longBreakSeconds},
        {QStringLiteral("cyclesBeforeLong"), p.cyclesBeforeLong},
        {QStringLiteral("builtin"), p.builtin},
        {QStringLiteral("summary"), summaryFor(p)},
        {QStringLiteral("detailedSummary"), detailedSummaryFor(p)},
        {QStringLiteral("iconName"), iconNameFor(p)},
        {QStringLiteral("workMinutes"), std::max(1, p.workSeconds / 60)},
        {QStringLiteral("options"), options},
    };
}

// --- Editing ---------------------------------------------------------------

QString PresetModel::create(const QVariantMap &fields)
{
    TimerPreset preset;
    preset.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    preset.name = i18n("New Timer");
    applyFields(preset, fields);

    const int row = count();
    beginInsertRows({}, row, row);
    m_presets.append(preset);
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT modified();
    return preset.uuid;
}

bool PresetModel::update(const QString &uuid, const QVariantMap &fields)
{
    const int row = indexOf(uuid);
    if (row < 0) {
        return false;
    }
    applyFields(m_presets[row], fields);
    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT presetChanged(uuid);
    if (uuid == m_currentUuid) {
        Q_EMIT currentChanged();
    }
    Q_EMIT modified();
    return true;
}

QString PresetModel::duplicate(const QString &uuid)
{
    const int source = indexOf(uuid);
    if (source < 0) {
        return {};
    }
    TimerPreset copy = m_presets.at(source);
    copy.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.builtin = false;
    copy.name = i18n("%1 (copy)", copy.name);

    const int row = source + 1;
    beginInsertRows({}, row, row);
    m_presets.insert(row, copy);
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT currentChanged(); // the current row index may have shifted
    Q_EMIT modified();
    return copy.uuid;
}

bool PresetModel::remove(const QString &uuid)
{
    const int row = indexOf(uuid);
    if (row < 0 || m_presets.at(row).builtin) {
        return false;
    }
    beginRemoveRows({}, row, row);
    m_presets.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();

    if (uuid == m_currentUuid) {
        m_currentUuid = DefaultUuid;
    }
    Q_EMIT currentChanged();
    Q_EMIT modified();
    return true;
}

bool PresetModel::reset(const QString &uuid)
{
    const int row = indexOf(uuid);
    if (row < 0 || !m_presets.at(row).builtin) {
        return false;
    }
    m_presets[row] = makeDefaultPreset();
    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT presetChanged(uuid);
    if (uuid == m_currentUuid) {
        Q_EMIT currentChanged();
    }
    Q_EMIT modified();
    return true;
}

// --- Options ---------------------------------------------------------------

QVariantList PresetModel::optionDescriptors() const
{
    return PresetSettings::descriptorsForQml();
}

QVariant PresetModel::option(const QString &uuid, const QString &key) const
{
    const int row = indexOf(uuid);
    return row >= 0 ? m_presets.at(row).option(key) : QVariant();
}

bool PresetModel::setOption(const QString &uuid, const QString &key, const QVariant &value)
{
    const int row = indexOf(uuid);
    if (row < 0 || !PresetSettings::isKnown(key)) {
        return false;
    }
    const std::optional<OptionDescriptor> descriptor = descriptorFor(key);
    if (!descriptor) {
        if (key != PresetOption::AutoStart) return false;
        const bool bVal = value.toBool();
        m_presets[row].options.insert(key, bVal);
        m_presets[row].options.insert(PresetOption::AutoStartMode, bVal ? 2 : 0);
    } else {
        const QVariant stored = coercedOption(*descriptor, value);
        m_presets[row].options.insert(key, stored);
        if (key == PresetOption::AutoStartMode) {
            m_presets[row].options.insert(PresetOption::AutoStart, stored.toInt() == 2);
        }
    }
    Q_EMIT dataChanged(index(row), index(row));
    Q_EMIT presetChanged(uuid);
    if (uuid == m_currentUuid) {
        Q_EMIT currentChanged();
    }
    Q_EMIT modified();
    return true;
}
