// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresetPersistence.h"

#include "PresetModel.h"
#include "PresetRepository.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPresetPersistence, "ktomato.db.persistence")

PresetPersistence::PresetPersistence(PresetModel *model, PresetRepository *repository, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_repository(repository)
{
    bool ok = false;
    const QList<TimerPreset> stored = m_repository->loadAll(&ok);
    if (ok && !stored.isEmpty()) {
        model->setPresets(stored, m_repository->currentUuid());
    } else if (ok) {
        // First run: store the built-in preset so the database is never empty afterwards.
        savePresets();
        saveSelection();
    } else {
        // Saving now would replace the stored presets (which we could not read) with the
        // built-in one, deleting the user's timers. Run with the built-in preset, unsaved.
        qCWarning(lcPresetPersistence) << "Cannot load the stored timers; changes will not be saved this session";
        return;
    }

    m_active = true;
    // Connect only after loading, so loading never writes back.
    connect(model, &PresetModel::modified, this, &PresetPersistence::savePresets);
    connect(model, &PresetModel::currentChanged, this, &PresetPersistence::saveSelection);
}

void PresetPersistence::savePresets()
{
    m_repository->saveAll(m_model->presets());
    saveSelection();
}

void PresetPersistence::saveSelection()
{
    m_repository->setCurrentUuid(m_model->currentUuid());
}
