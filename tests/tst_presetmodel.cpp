// SPDX-License-Identifier: GPL-3.0-or-later
#include <QSignalSpy>
#include <QTest>

#include <KLocalizedString>

#include <memory>

#include "PresetModel.h"

class PresetModelTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<PresetModel> model;

    QString nameAt(int row) const
    {
        return model->data(model->index(row), PresetModel::NameRole).toString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("ktomato");
    }

    void init()
    {
        model = std::make_unique<PresetModel>();
    }

    void defaultPresetIsPresentAndCurrent()
    {
        QCOMPARE(model->count(), 1);
        QCOMPARE(model->currentUuid(), PresetModel::DefaultUuid);
        QCOMPARE(model->currentIndex(), 0);

        const QVariantMap d = model->get(PresetModel::DefaultUuid);
        QCOMPARE(d.value(QStringLiteral("workSeconds")).toInt(), 25 * 60);
        QCOMPARE(d.value(QStringLiteral("shortBreakSeconds")).toInt(), 5 * 60);
        QCOMPARE(d.value(QStringLiteral("longBreakSeconds")).toInt(), 15 * 60);
        QCOMPARE(d.value(QStringLiteral("cyclesBeforeLong")).toInt(), 4);
        QVERIFY(d.value(QStringLiteral("builtin")).toBool());
    }

    void createAddsRowAndClampsValues()
    {
        QSignalSpy inserted(model.get(), &QAbstractItemModel::rowsInserted);
        const QString uuid = model->create({
            {QStringLiteral("name"), QStringLiteral("  Deep work ")},
            {QStringLiteral("category"), QStringLiteral(" Work ")},
            {QStringLiteral("workSeconds"), 50 * 60},
            {QStringLiteral("shortBreakSeconds"), -10},
            {QStringLiteral("cyclesBeforeLong"), 1000},
        });

        QVERIFY(!uuid.isEmpty());
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(model->count(), 2);

        const QVariantMap p = model->get(uuid);
        QCOMPARE(p.value(QStringLiteral("name")).toString(), QStringLiteral("Deep work"));
        QCOMPARE(p.value(QStringLiteral("category")).toString(), QStringLiteral("Work"));
        QCOMPARE(p.value(QStringLiteral("workSeconds")).toInt(), 50 * 60);
        QCOMPARE(p.value(QStringLiteral("shortBreakSeconds")).toInt(), 0);
        QCOMPARE(p.value(QStringLiteral("cyclesBeforeLong")).toInt(), 99);
        QVERIFY(!p.value(QStringLiteral("builtin")).toBool());
    }

    void createWithBlankNameGetsFallbackName()
    {
        const QString uuid = model->create({{QStringLiteral("name"), QStringLiteral("   ")}});
        QVERIFY(!model->get(uuid).value(QStringLiteral("name")).toString().isEmpty());
    }

    void updateChangesFieldsAndNotifies()
    {
        const QString uuid = model->create({{QStringLiteral("name"), QStringLiteral("A")}});
        QSignalSpy changed(model.get(), &QAbstractItemModel::dataChanged);
        QSignalSpy presetChanged(model.get(), &PresetModel::presetChanged);

        QVERIFY(model->update(uuid, {{QStringLiteral("name"), QStringLiteral("B")},
                                     {QStringLiteral("workSeconds"), 10 * 60}}));
        QCOMPARE(nameAt(1), QStringLiteral("B"));
        QCOMPARE(model->get(uuid).value(QStringLiteral("workSeconds")).toInt(), 10 * 60);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(presetChanged.count(), 1);
        QCOMPARE(presetChanged.at(0).at(0).toString(), uuid);

        QVERIFY(!model->update(QStringLiteral("missing"), {}));
    }

    void createAndApplyFieldsWithOptions()
    {
        const QString uuid = model->create({
            {QStringLiteral("name"), QStringLiteral("Custom")},
            {PresetOption::AutoStartMode, 1},
            {PresetOption::SilenceWhileWorking, true},
            {PresetOption::SoundOnEnd, false},
            {PresetOption::NotifyOnEnd, true}
        });

        QCOMPARE(model->option(uuid, PresetOption::AutoStartMode).toInt(), 1);
        QCOMPARE(model->option(uuid, PresetOption::AutoStart).toBool(), false);
        QCOMPARE(model->option(uuid, PresetOption::SilenceWhileWorking).toBool(), true);
        QCOMPARE(model->option(uuid, PresetOption::SoundOnEnd).toBool(), false);
        QCOMPARE(model->option(uuid, PresetOption::NotifyOnEnd).toBool(), true);

        // Update with new options
        QVERIFY(model->update(uuid, {
            {PresetOption::AutoStartMode, 2},
            {PresetOption::SilenceWhileWorking, false}
        }));
        QCOMPARE(model->option(uuid, PresetOption::AutoStartMode).toInt(), 2);
        QCOMPARE(model->option(uuid, PresetOption::AutoStart).toBool(), true);
        QCOMPARE(model->option(uuid, PresetOption::SilenceWhileWorking).toBool(), false);
    }

    void duplicateCopiesFieldsAndOptionsAndIsNeverBuiltin()
    {
        QVERIFY(model->setOption(PresetModel::DefaultUuid, PresetOption::SilenceWhileWorking, true));
        const QString copyUuid = model->duplicate(PresetModel::DefaultUuid);

        QVERIFY(!copyUuid.isEmpty());
        QVERIFY(copyUuid != PresetModel::DefaultUuid);
        QCOMPARE(model->count(), 2);
        QCOMPARE(model->indexOf(copyUuid), 1); // right after its source

        const QVariantMap copy = model->get(copyUuid);
        QVERIFY(!copy.value(QStringLiteral("builtin")).toBool());
        QVERIFY(copy.value(QStringLiteral("name")).toString().contains(nameAt(0)));
        QCOMPARE(copy.value(QStringLiteral("workSeconds")).toInt(), 25 * 60);
        QVERIFY(model->option(copyUuid, PresetOption::SilenceWhileWorking).toBool());
    }

    void duplicatingUnknownUuidFails()
    {
        QVERIFY(model->duplicate(QStringLiteral("missing")).isEmpty());
        QCOMPARE(model->count(), 1);
    }

    void builtinCannotBeRemoved()
    {
        QVERIFY(!model->remove(PresetModel::DefaultUuid));
        QCOMPARE(model->count(), 1);
    }

    void removingCurrentFallsBackToDefault()
    {
        const QString uuid = model->create({{QStringLiteral("name"), QStringLiteral("Temp")}});
        model->setCurrentUuid(uuid);
        QCOMPARE(model->currentUuid(), uuid);

        QSignalSpy current(model.get(), &PresetModel::currentChanged);
        QVERIFY(model->remove(uuid));
        QCOMPARE(model->count(), 1);
        QCOMPARE(model->currentUuid(), PresetModel::DefaultUuid);
        QVERIFY(current.count() >= 1);
    }

    void removingOtherPresetKeepsSelection()
    {
        const QString a = model->create({{QStringLiteral("name"), QStringLiteral("A")}});
        const QString b = model->create({{QStringLiteral("name"), QStringLiteral("B")}});
        model->setCurrentUuid(b);
        QVERIFY(model->remove(a));
        QCOMPARE(model->currentUuid(), b);
        QCOMPARE(model->currentIndex(), 1);
    }

    void unknownCurrentUuidIsIgnored()
    {
        model->setCurrentUuid(QStringLiteral("missing"));
        QCOMPARE(model->currentUuid(), PresetModel::DefaultUuid);
    }

    void optionsFallBackToDefaultsAndRoundTrip()
    {
        const QString uuid = PresetModel::DefaultUuid;
        QVERIFY(model->option(uuid, PresetOption::SoundOnEnd).toBool());          // default on
        QVERIFY(!model->option(uuid, PresetOption::AutoStart).toBool());          // default off
        QVERIFY(!model->option(uuid, PresetOption::SilenceWhileWorking).toBool());

        QSignalSpy presetChanged(model.get(), &PresetModel::presetChanged);
        QVERIFY(model->setOption(uuid, PresetOption::AutoStart, true));
        QVERIFY(model->option(uuid, PresetOption::AutoStart).toBool());
        QCOMPARE(presetChanged.count(), 1);

        const QVariantMap options = model->get(uuid).value(QStringLiteral("options")).toMap();
        QVERIFY(options.value(PresetOption::AutoStart).toBool());
        QVERIFY(options.contains(PresetOption::NotifyOnEnd)); // defaults merged in
    }

    void unknownOptionIsRejected()
    {
        QVERIFY(!model->setOption(PresetModel::DefaultUuid, QStringLiteral("bogus"), true));
        QVERIFY(!model->setOption(QStringLiteral("missing"), PresetOption::AutoStart, true));
    }

    void everyDescriptorHasLabelAndDefault()
    {
        const QVariantList list = model->optionDescriptors();
        QVERIFY(list.size() >= 4);
        bool foundAutoStartMode = false;
        for (const QVariant &entry : list) {
            const QVariantMap d = entry.toMap();
            QVERIFY(!d.value(QStringLiteral("key")).toString().isEmpty());
            QVERIFY(!d.value(QStringLiteral("label")).toString().isEmpty());
            QVERIFY(d.value(QStringLiteral("defaultValue")).isValid());
            const QString key = d.value(QStringLiteral("key")).toString();
            if (key == PresetOption::AutoStartMode) {
                QCOMPARE(d.value(QStringLiteral("type")).toString(), QStringLiteral("int"));
                QCOMPARE(d.value(QStringLiteral("defaultValue")).toInt(), 0);
                foundAutoStartMode = true;
            } else {
                QCOMPARE(d.value(QStringLiteral("type")).toString(), QStringLiteral("bool"));
            }
        }
        QVERIFY(foundAutoStartMode);
    }

    void resetRestoresBuiltinOnly()
    {
        const QString uuid = PresetModel::DefaultUuid;
        model->update(uuid, {{QStringLiteral("workSeconds"), 99 * 60}, {QStringLiteral("name"), QStringLiteral("X")}});
        model->setOption(uuid, PresetOption::AutoStart, true);

        QVERIFY(model->reset(uuid));
        const QVariantMap d = model->get(uuid);
        QCOMPARE(d.value(QStringLiteral("workSeconds")).toInt(), 25 * 60);
        QVERIFY(!model->option(uuid, PresetOption::AutoStart).toBool());

        const QString custom = model->create({});
        QVERIFY(!model->reset(custom));
    }

    void rolesExposeSummaryAndBuiltinFlag()
    {
        const QModelIndex idx = model->index(0);
        QVERIFY(model->data(idx, PresetModel::BuiltinRole).toBool());
        const QString summary = model->data(idx, PresetModel::SummaryRole).toString();
        QVERIFY(summary.contains(QStringLiteral("25")));
        QVERIFY(summary.contains(QStringLiteral("5")));
        QVERIFY(model->roleNames().values().contains("uuid"));
        QVERIFY(model->roleNames().values().contains("detailedSummary"));
        QVERIFY(model->roleNames().values().contains("iconName"));
        QVERIFY(model->roleNames().values().contains("workMinutes"));
    }

    void standardPresetsAndAddStandardPresets()
    {
        QCOMPARE(model->count(), 1);
        QCOMPARE(PresetModel::standardPresets().size(), 4);

        model->addStandardPresets();
        QCOMPARE(model->count(), 4);

        const int deepWorkIdx = model->indexOf(QStringLiteral("deep-work"));
        QVERIFY(deepWorkIdx >= 0);
        QCOMPARE(model->data(model->index(deepWorkIdx), PresetModel::WorkSecondsRole).toInt(), 50 * 60);
        QCOMPARE(model->data(model->index(deepWorkIdx), PresetModel::WorkMinutesRole).toInt(), 50);
        QCOMPARE(model->data(model->index(deepWorkIdx), PresetModel::IconNameRole).toString(), QStringLiteral("flash-symbolic"));

        const int studyIdx = model->indexOf(QStringLiteral("study"));
        QVERIFY(studyIdx >= 0);
        QCOMPARE(model->data(model->index(studyIdx), PresetModel::WorkSecondsRole).toInt(), 45 * 60);
        QCOMPARE(model->data(model->index(studyIdx), PresetModel::IconNameRole).toString(), QStringLiteral("view-readermode-symbolic"));

        const int sprintIdx = model->indexOf(QStringLiteral("quick-sprint"));
        QVERIFY(sprintIdx >= 0);
        QCOMPARE(model->data(model->index(sprintIdx), PresetModel::WorkSecondsRole).toInt(), 15 * 60);
        QCOMPARE(model->data(model->index(sprintIdx), PresetModel::IconNameRole).toString(), QStringLiteral("speedometer-symbolic"));

        // Calling it again does not duplicate entries
        model->addStandardPresets();
        QCOMPARE(model->count(), 4);
    }

    void currentPropertiesReflectSelectedPreset()
    {
        model->addStandardPresets();
        QCOMPARE(model->currentUuid(), PresetModel::DefaultUuid);
        QCOMPARE(model->currentWorkMinutes(), 25);
        QVERIFY(!model->currentDetailedSummary().isEmpty());

        model->setCurrentUuid(QStringLiteral("deep-work"));
        QCOMPARE(model->currentWorkMinutes(), 50);
        QCOMPARE(model->currentName(), QStringLiteral("Deep Work"));
        QCOMPARE(model->currentIconName(), QStringLiteral("flash-symbolic"));
    }

    void customIconCanBeSelectedAndOverridesDefault()
    {
        const QVariantList icons = model->availableIcons();
        QVERIFY(!icons.isEmpty());
        QVERIFY(icons.size() >= 10);

        const QString customUuid = model->create({
            {QStringLiteral("name"), QStringLiteral("Reading Session")},
            {QStringLiteral("iconName"), QStringLiteral("bookmarks-symbolic")}
        });

        const int row = model->indexOf(customUuid);
        QVERIFY(row >= 0);
        QCOMPARE(model->data(model->index(row), PresetModel::IconNameRole).toString(), QStringLiteral("bookmarks-symbolic"));

        model->update(customUuid, {{QStringLiteral("iconName"), QStringLiteral("flag-symbolic")}});
        QCOMPARE(model->data(model->index(row), PresetModel::IconNameRole).toString(), QStringLiteral("flag-symbolic"));
    }

    void legacyAutoStartOptionMapsToAutoStartMode()
    {
        TimerPreset legacy;
        legacy.uuid = QStringLiteral("legacy-preset");
        legacy.name = QStringLiteral("Legacy");
        legacy.options.insert(PresetOption::AutoStart, true);

        model->setPresets({legacy}, legacy.uuid);
        // option(AutoStartMode) must return 2 (All) when legacy AutoStart was true
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStartMode).toInt(), 2);
        // get() options must contain AutoStartMode = 2
        const QVariantMap opts = model->get(legacy.uuid).value(QStringLiteral("options")).toMap();
        QCOMPARE(opts.value(PresetOption::AutoStartMode).toInt(), 2);

        // Setting AutoStartMode to 0 (Manual) updates legacy AutoStart to false
        model->setOption(legacy.uuid, PresetOption::AutoStartMode, 0);
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStartMode).toInt(), 0);
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStart).toBool(), false);

        // Setting AutoStartMode to 1 (BreaksOnly)
        model->setOption(legacy.uuid, PresetOption::AutoStartMode, 1);
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStartMode).toInt(), 1);
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStart).toBool(), false);

        // Setting AutoStartMode to 2 (All) updates legacy AutoStart to true
        model->setOption(legacy.uuid, PresetOption::AutoStartMode, 2);
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStartMode).toInt(), 2);
        QCOMPARE(model->option(legacy.uuid, PresetOption::AutoStart).toBool(), true);
    }

    void summariesShowSecondsForSubMinuteDurations()
    {
        const QString uuid = model->create({{QStringLiteral("workSeconds"), 30}, {QStringLiteral("shortBreakSeconds"), 90}});
        const QString summary = model->get(uuid).value(QStringLiteral("summary")).toString();
        QVERIFY2(!summary.startsWith(QStringLiteral("0 min")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("30")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("1 min 30")), qPrintable(summary));
    }

    void setOptionNotifiesTheRow()
    {
        QSignalSpy changed(model.get(), &QAbstractItemModel::dataChanged);
        QVERIFY(model->setOption(PresetModel::DefaultUuid, PresetOption::SoundOnEnd, false));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().at(0).toModelIndex().row(), 0);
    }

    void intOptionsAreClampedToTheirOwnRange()
    {
        const QString uuid = PresetModel::DefaultUuid;
        QVERIFY(model->setOption(uuid, PresetOption::AutoStartMode, 7));
        QCOMPARE(model->option(uuid, PresetOption::AutoStartMode).toInt(), 2);
        QVERIFY(model->setOption(uuid, PresetOption::AutoStartMode, -3));
        QCOMPARE(model->option(uuid, PresetOption::AutoStartMode).toInt(), 0);

        model->update(uuid, {{PresetOption::AutoStartMode, 42}});
        QCOMPARE(model->option(uuid, PresetOption::AutoStartMode).toInt(), 2);

        const QVariantList list = model->optionDescriptors();
        for (const QVariant &entry : list) {
            const QVariantMap d = entry.toMap();
            if (d.value(QStringLiteral("key")).toString() == PresetOption::AutoStartMode) {
                QCOMPARE(d.value(QStringLiteral("minimum")).toInt(), 0);
                QCOMPARE(d.value(QStringLiteral("maximum")).toInt(), 2);
            }
        }
    }

    void legacyDefaultNameIsRenamed()
    {
        TimerPreset legacy;
        legacy.uuid = PresetModel::DefaultUuid;
        legacy.name = QStringLiteral("Default");
        model->setPresets({legacy}, legacy.uuid);
        model->addStandardPresets();
        QCOMPARE(model->currentName(), PresetModel::standardPresets().first().name);

        // A user's own name is kept.
        model->update(PresetModel::DefaultUuid, {{QStringLiteral("name"), QStringLiteral("Mine")}});
        model->addStandardPresets();
        QCOMPARE(model->currentName(), QStringLiteral("Mine"));
    }
};

QTEST_GUILESS_MAIN(PresetModelTest)
#include "tst_presetmodel.moc"
