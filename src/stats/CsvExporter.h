// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QTimeZone>

#include "SessionRepository.h"

class QIODevice;

/**
 * Writes sessions as CSV (RFC 4180: comma separated, CRLF line ends, quoted where needed,
 * UTF-8 with a byte order mark). One row per phase; times are ISO 8601 in local time with the UTC offset, so the
 * file is unambiguous across daylight-saving changes.
 */
namespace CsvExporter
{
/// Returns false if writing failed.
bool write(QIODevice &out, const QList<SessionRecord> &records, const QTimeZone &timeZone);

/// One field, quoted if it contains a comma, quote or line break. Text that a spreadsheet
/// would run as a formula (= + - @ as the first non-blank character, or a leading tab or
/// carriage return) gets a leading
/// apostrophe, because timer names are typed by the user and the file will be opened in one.
QString escapeField(const QString &text);
} // namespace CsvExporter
