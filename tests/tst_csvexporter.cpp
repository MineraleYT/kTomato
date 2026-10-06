// SPDX-License-Identifier: GPL-3.0-or-later
#include <QBuffer>
#include <QTest>
#include <QTimeZone>

#include "CsvExporter.h"

class TestCsvExporter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEscapeField();
    void testWriteCsv();
};

void TestCsvExporter::testEscapeField()
{
    // Normal text untouched
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("Work")), QStringLiteral("Work"));

    // Quotes and commas escaped and quoted
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("Study, Reading")), QStringLiteral("\"Study, Reading\""));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("He said \"Hello\"")), QStringLiteral("\"He said \"\"Hello\"\"\""));

    // Spreadsheet formula injection protection
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("=SUM(A1:A10)")), QStringLiteral("'=SUM(A1:A10)"));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("+1234")), QStringLiteral("'+1234"));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("-cmd")), QStringLiteral("'-cmd"));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("@mention")), QStringLiteral("'@mention"));

    // Spreadsheets skip leading blanks, non-breaking spaces included, before looking for a formula.
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("  =1+1")), QStringLiteral("'  =1+1"));
    QCOMPARE(CsvExporter::escapeField(QString(QChar(0x00A0)) + QStringLiteral("@SUM(A1)")),
             QStringLiteral("'") + QChar(0x00A0) + QStringLiteral("@SUM(A1)"));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("\t-2")), QStringLiteral("'\t-2"));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("\rx")), QStringLiteral("\"'\rx\""));
    // Harmless text that merely contains those characters later is left alone.
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("  a=b")), QStringLiteral("  a=b"));
    QCOMPARE(CsvExporter::escapeField(QStringLiteral("   ")), QStringLiteral("   "));
}

void TestCsvExporter::testWriteCsv()
{
    const QTimeZone tz = QTimeZone::utc();
    SessionRecord r;
    r.kind = SessionKind::Work;
    r.startedAtMs = QDateTime(QDate(2026, 10, 3), QTime(14, 0), tz).toMSecsSinceEpoch();
    r.endedAtMs = QDateTime(QDate(2026, 10, 3), QTime(14, 25), tz).toMSecsSinceEpoch();
    r.durationSec = 1500;
    r.plannedSec = 1500;
    r.completed = true;
    r.presetName = QStringLiteral("Pomodoro, Standard");
    r.category = QStringLiteral("Coding");

    r.note = QStringLiteral("Fix bug #12");

    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::ReadWrite));
    QVERIFY(CsvExporter::write(buffer, {r}, tz));

    buffer.seek(0);
    const QByteArray bytes = buffer.readAll();
    // UTF-8 byte order mark first, so spreadsheets do not guess a legacy encoding.
    QVERIFY(bytes.startsWith("\xEF\xBB\xBF"));
    const QString content = QString::fromUtf8(bytes.mid(3));
    QVERIFY(!content.startsWith(QChar(0xFEFF))); // exactly one BOM
    const QStringList lines = content.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);

    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0), QStringLiteral("start,end,phase,active_seconds,planned_seconds,completed,timer,category,note"));
    QCOMPARE(lines.at(1), QStringLiteral("2026-10-03T14:00:00+00:00,2026-10-03T14:25:00+00:00,work,1500,1500,yes,\"Pomodoro, Standard\",Coding,Fix bug #12"));
}

QTEST_MAIN(TestCsvExporter)
#include "tst_csvexporter.moc"
