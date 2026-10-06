// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class PresetModel;
class PresetRepository;

/**
 * Loads the presets into the model at construction and saves every later change.
 *
 * Saving is synchronous: a handful of rows in a WAL database costs well under a
 * millisecond, and it means a crash can lose at most the edit in progress.
 */
class PresetPersistence : public QObject
{
    Q_OBJECT

public:
    PresetPersistence(PresetModel *model, PresetRepository *repository, QObject *parent = nullptr);

    /// True when the stored timers were read (or none existed yet) and changes will be saved.
    /// False when loading failed: the model then only holds the built-in timer, unsaved.
    bool isActive() const { return m_active; }

private:
    void savePresets();
    void saveSelection();

    PresetModel *m_model;
    PresetRepository *m_repository;
    bool m_active = false;
};
