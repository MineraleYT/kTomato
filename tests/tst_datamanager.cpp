// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <QTemporaryDir>
#include <QTest>

#include <KLocalizedString>

#include "DataManager.h"
#include "Database.h"
#include "FakeClock.h"
#include "PresetModel.h"
#include "PresetRepository.h"
#include "SessionRecorder.h"
#include "SessionRepository.h"
#include "TimerEngine.h"

#include <limits>
#include <memory>

namespace
{
/// Runs statements on a database file through a throw-away connection.
bool execOn(const QString &path, const QStringList &statements)
{
    const QString name = QStringLiteral("edit-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        ok = db.open();
        QSqlQuery query(db);
        for (const QString &statement : statements) {
            ok = ok && query.exec(statement);
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return ok;
}

int userVersionOf(const QString &path)
{
    const QString name = QStringLiteral("check-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    int version = -1;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) {
                version = query.value(0).toInt();
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return version;
}
} // namespace

class DataManagerTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir dir;
    std::unique_ptr<Database> database;
    std::unique_ptr<SessionRepository> sessions;
    std::unique_ptr<PresetRepository> presetRepository;
    std::unique_ptr<PresetModel> presetModel;
    std::unique_ptr<DataManager> manager;

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setApplicationDomain("ktomato");
    }

    void init()
    {
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("active.db"));
        QFile::remove(path);
        for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")}) {
            QFile::remove(dir.filePath(QStringLiteral("backup.db")) + suffix);
        }
        database = std::make_unique<Database>(path);
        QVERIFY(database->open());
        sessions = std::make_unique<SessionRepository>(database->connectionName());
        presetRepository = std::make_unique<PresetRepository>(database->connectionName());
        presetModel = std::make_unique<PresetModel>();
        TimerPreset preset;
        preset.uuid = QStringLiteral("persisted-preset");
        preset.name = QStringLiteral("Persisted preset");
        preset.category = QStringLiteral("Test");
        QVERIFY(presetRepository->saveAll({preset}));
        QVERIFY(presetRepository->setCurrentUuid(preset.uuid));
        presetModel->setPresets({preset}, preset.uuid);
        manager = std::make_unique<DataManager>(database.get(), sessions.get(), presetRepository.get(), presetModel.get(), nullptr);
    }

    void cleanup()
    {
        manager.reset();
        presetModel.reset();
        presetRepository.reset();
        sessions.reset();
        database.reset();
    }

    void backupCanBeRestoredAfterHistoryIsCleared()
    {
        SessionRecord record;
        record.kind = SessionKind::Work;
        record.startedAtMs = 1000;
        record.endedAtMs = 1600;
        record.durationSec = 600;
        record.plannedSec = 1500;
        record.completed = true;
        record.presetName = QStringLiteral("Pomodoro");
        QVERIFY(sessions->insert(record));
        QCOMPARE(manager->sessionCount(), 1);

        QSignalSpy succeeded(manager.get(), &DataManager::operationSucceeded);
        const QUrl backup = QUrl::fromLocalFile(dir.filePath(QStringLiteral("backup.db")));
        QVERIFY(manager->backupDatabase(backup));
        QVERIFY(QFile::exists(backup.toLocalFile()));
        QCOMPARE(succeeded.count(), 1);

        QVERIFY(manager->clearHistory());
        QCOMPARE(manager->sessionCount(), 0);
        QCOMPARE(succeeded.count(), 2);

        QVERIFY(presetRepository->saveAll({}));
        presetModel->setPresets({}, QString());

        QVERIFY(manager->restoreDatabase(backup));
        QCOMPARE(manager->sessionCount(), 1);
        const auto restored = sessions->between(0, 2000);
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.first().presetName, QStringLiteral("Pomodoro"));
        QCOMPARE(restored.first().plannedSec, 1500);
        QCOMPARE(presetModel->count(), 2); // built-in default plus the restored custom preset
        QCOMPARE(presetModel->get(QStringLiteral("persisted-preset")).value(QStringLiteral("name")).toString(),
                 QStringLiteral("Persisted preset"));
    }

    void invalidBackupAndActiveDatabaseAreRejectedWithoutDataLoss()
    {
        SessionRecord record;
        record.startedAtMs = 100;
        record.endedAtMs = 200;
        record.presetName = QStringLiteral("Existing");
        QVERIFY(sessions->insert(record));
        QSignalSpy failed(manager.get(), &DataManager::operationFailed);

        const QString invalidPath = dir.filePath(QStringLiteral("invalid.db"));
        QFile invalid(invalidPath);
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("not a database");
        invalid.close();
        QVERIFY(!manager->restoreDatabase(QUrl::fromLocalFile(invalidPath)));
        QVERIFY(!manager->restoreDatabase(QUrl::fromLocalFile(database->path())));
        QVERIFY(!manager->backupDatabase(QUrl::fromLocalFile(database->path())));

        QCOMPARE(manager->sessionCount(), 1);
        QCOMPARE(sessions->between(0, 1000).first().presetName, QStringLiteral("Existing"));
        QCOMPARE(failed.count(), 3);
    }

    SessionRecord sampleRecord(const QString &name = QStringLiteral("Pomodoro"))
    {
        SessionRecord record;
        record.kind = SessionKind::Work;
        record.startedAtMs = 1000;
        record.endedAtMs = 1600;
        record.durationSec = 600;
        record.plannedSec = 1500;
        record.completed = true;
        record.presetName = name;
        record.note = QStringLiteral("Backed-up note");
        return record;
    }

    void backupWithOlderSchemaIsMigratedAndRestored()
    {
        QVERIFY(sessions->insert(sampleRecord()));
        const QString backupPath = dir.filePath(QStringLiteral("backup.db"));
        QVERIFY(manager->backupDatabase(QUrl::fromLocalFile(backupPath)));
        // Turn the backup into a schema 1 file, as kTomato 1.0 wrote it (no note column).
        QVERIFY(execOn(backupPath, {QStringLiteral("ALTER TABLE session DROP COLUMN note"),
                                    QStringLiteral("PRAGMA user_version = 1")}));
        QCOMPARE(userVersionOf(backupPath), 1);
        QVERIFY(manager->clearHistory());

        QSignalSpy failed(manager.get(), &DataManager::operationFailed);
        QVERIFY(manager->restoreDatabase(QUrl::fromLocalFile(backupPath)));
        QCOMPARE(failed.count(), 0);
        const auto restored = sessions->between(0, 2000);
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.first().presetName, QStringLiteral("Pomodoro"));
        QVERIFY(restored.first().note.isEmpty());

        // The user's file is read only: not migrated, no journal files left next to it.
        QCOMPARE(userVersionOf(backupPath), 1);
        QVERIFY(!QFile::exists(backupPath + QStringLiteral("-wal")));
        QVERIFY(!QFile::exists(backupPath + QStringLiteral("-shm")));
    }

    void backupFromNewerOrForeignDatabaseIsRejected()
    {
        QVERIFY(sessions->insert(sampleRecord(QStringLiteral("Existing"))));
        const QString backupPath = dir.filePath(QStringLiteral("backup.db"));
        QVERIFY(manager->backupDatabase(QUrl::fromLocalFile(backupPath)));
        QSignalSpy failed(manager.get(), &DataManager::operationFailed);

        QVERIFY(execOn(backupPath, {QStringLiteral("PRAGMA user_version = %1").arg(Database::latestSchemaVersion() + 1)}));
        QVERIFY(!manager->restoreDatabase(QUrl::fromLocalFile(backupPath)));

        QVERIFY(execOn(backupPath, {QStringLiteral("PRAGMA user_version = %1").arg(Database::latestSchemaVersion()),
                                    QStringLiteral("PRAGMA application_id = 42")}));
        QVERIFY(!manager->restoreDatabase(QUrl::fromLocalFile(backupPath)));

        QCOMPARE(failed.count(), 2);
        QCOMPARE(manager->sessionCount(), 1);
        QCOMPARE(sessions->between(0, 2000).first().presetName, QStringLiteral("Existing"));
    }

    void missingBackupFileIsNotCreated()
    {
        const QString missing = dir.filePath(QStringLiteral("does-not-exist.db"));
        QVERIFY(!manager->restoreDatabase(QUrl::fromLocalFile(missing)));
        QVERIFY(!QFile::exists(missing));
    }

    void recorderKeepsSessionCountAndLastWorkSessionIdCurrent()
    {
        QCOMPARE(manager->lastWorkSessionId(), -1); // no recorder yet

        FakeClock clock;
        TimerEngine engine(&clock, nullptr);
        SessionRecorder recorder(&engine, presetModel.get(), sessions.get());
        manager->setSessionRecorder(&recorder);
        QSignalSpy changed(manager.get(), &DataManager::dataChanged);

        engine.start();
        clock.advanceSeconds(60);
        engine.refresh();
        engine.stop();

        QCOMPARE(changed.count(), 1);
        QCOMPARE(manager->sessionCount(), 1);
        const qint64 id = manager->lastWorkSessionId();
        QCOMPARE(id, sessions->between(0, std::numeric_limits<qint64>::max()).first().id);

        QVERIFY(manager->updateSessionNote(id, QStringLiteral("Wrote tests")));
        QCOMPARE(manager->recentTaskNotes(), QStringList{QStringLiteral("Wrote tests")});

        manager->setSessionRecorder(nullptr);
        QCOMPARE(manager->lastWorkSessionId(), -1);
    }

    void temporaryDatabaseIsReported()
    {
        QVERIFY(!manager->usingTemporaryDatabase());
        Database memory(QStringLiteral(":memory:"));
        QVERIFY(memory.open());
        DataManager inMemory(&memory, nullptr, nullptr, nullptr, nullptr);
        QVERIFY(inMemory.usingTemporaryDatabase());
    }

    void presetCountReflectsModelCount()
    {
        QCOMPARE(manager->presetCount(), presetModel->count());
        QCOMPARE(manager->presetCount(), 2);

        const QString newUuid = presetModel->create({{QStringLiteral("name"), QStringLiteral("Second")}});
        QCOMPARE(manager->presetCount(), 3);

        presetModel->remove(newUuid);
        QCOMPARE(manager->presetCount(), 2);
    }
};

QTEST_GUILESS_MAIN(DataManagerTest)
#include "tst_datamanager.moc"
