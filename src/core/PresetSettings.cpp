// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresetSettings.h"

#include <KLocalizedString>

#include <algorithm>

QList<OptionDescriptor> PresetSettings::descriptors()
{
    // Built on every call so labels follow the current language.
    return {
        {PresetOption::SilenceWhileWorking,
         i18n("Silence notifications while working"),
         i18n("Turns off all system notifications for the whole work phase and turns them back on during the break."),
         false,
         QStringLiteral("bool")},
        {PresetOption::AutoStartMode,
         i18n("Auto-start mode"),
         i18n("Choose whether breaks, work phases, or neither start automatically."),
         0,
         QStringLiteral("int"),
         0,
         2},
        {PresetOption::SoundOnEnd,
         i18n("Play a sound when a phase ends"),
         QString(),
         true,
         QStringLiteral("bool")},
        {PresetOption::NotifyOnEnd,
         i18n("Show a notification when a phase ends"),
         QString(),
         true,
         QStringLiteral("bool")},
    };
}

bool PresetSettings::isKnown(const QString &key)
{
    // Kept readable for presets saved before AutoStartMode existed.
    if (key == PresetOption::AutoStart) {
        return true;
    }
    const auto all = descriptors();
    return std::any_of(all.cbegin(), all.cend(), [&](const OptionDescriptor &d) {
        return d.key == key;
    });
}

QVariant PresetSettings::defaultValue(const QString &key)
{
    const auto all = descriptors();
    for (const OptionDescriptor &d : all) {
        if (d.key == key) {
            return d.defaultValue;
        }
    }
    return {};
}

QVariantMap PresetSettings::defaults()
{
    QVariantMap map;
    const auto all = descriptors();
    for (const OptionDescriptor &d : all) {
        map.insert(d.key, d.defaultValue);
    }
    return map;
}

QVariantList PresetSettings::descriptorsForQml()
{
    QVariantList list;
    const auto all = descriptors();
    for (const OptionDescriptor &d : all) {
        list.append(QVariantMap{
            {QStringLiteral("key"), d.key},
            {QStringLiteral("label"), d.label},
            {QStringLiteral("description"), d.description},
            {QStringLiteral("defaultValue"), d.defaultValue},
            {QStringLiteral("type"), d.type},
            {QStringLiteral("minimum"), d.minimum},
            {QStringLiteral("maximum"), d.maximum},
        });
    }
    return list;
}
