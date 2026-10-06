// SPDX-License-Identifier: GPL-3.0-or-later
#include "CsvExporter.h"

#include <QDateTime>
#include <QIODevice>
#include <QStringList>

namespace CsvExporter
{
QString escapeField(const QString &text)
{
    QString field = text;

    // CSV injection: a cell that starts with one of these is evaluated by spreadsheets, which
    // also skip leading blanks (including non-breaking spaces) before looking.
    static const QString formulaStarts = QStringLiteral("=+-@");
    qsizetype first = 0;
    while (first < field.size() && field.at(first).isSpace()) {
        ++first;
    }
    const bool startsWithControl = !field.isEmpty() && (field.at(0) == QLatin1Char('\t') || field.at(0) == QLatin1Char('\r'));
    if (startsWithControl || (first < field.size() && formulaStarts.contains(field.at(first)))) {
        field.prepend(QLatin1Char('\''));
    }

    const bool needsQuotes = field.contains(QLatin1Char(',')) || field.contains(QLatin1Char('"'))
        || field.contains(QLatin1Char('\n')) || field.contains(QLatin1Char('\r'));
    if (needsQuotes) {
        field.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        field = QLatin1Char('"') + field + QLatin1Char('"');
    }
    return field;
}

bool write(QIODevice &out, const QList<SessionRecord> &records, const QTimeZone &timeZone)
{
    auto writeLine = [&out](const QStringList &fields) {
        const QByteArray line = fields.join(QLatin1Char(',')).toUtf8() + "\r\n";
        return out.write(line) == line.size();
    };

    // A byte order mark, so spreadsheet programs (Excel above all) read the file as UTF-8
    // instead of a legacy code page and show accented timer names correctly.
    static const QByteArray bom("\xEF\xBB\xBF");
    if (out.write(bom) != bom.size()) {
        return false;
    }

    if (!writeLine({QStringLiteral("start"), QStringLiteral("end"), QStringLiteral("phase"),
                    QStringLiteral("active_seconds"), QStringLiteral("planned_seconds"),
                    QStringLiteral("completed"), QStringLiteral("timer"), QStringLiteral("category"),
                    QStringLiteral("note")})) {
        return false;
    }

    for (const SessionRecord &r : records) {
        const QString start = QDateTime::fromMSecsSinceEpoch(r.startedAtMs, timeZone).toString(Qt::ISODate);
        const QString end = QDateTime::fromMSecsSinceEpoch(r.endedAtMs, timeZone).toString(Qt::ISODate);
        if (!writeLine({start,
                        end,
                        SessionRepository::kindToString(r.kind),
                        QString::number(r.durationSec),
                        QString::number(r.plannedSec),
                        r.completed ? QStringLiteral("yes") : QStringLiteral("no"),
                        escapeField(r.presetName),
                        escapeField(r.category),
                        escapeField(r.note)})) {
            return false;
        }
    }
    return true;
}
} // namespace CsvExporter
