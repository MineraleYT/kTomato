// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QTimeZone>

#include "Clock.h"

/// Manually advanced clock; wall time follows the monotonic time.
class FakeClock final : public Clock
{
public:
    qint64 monotonicMs() const override { return m_ms; }
    QDateTime wallTime() const override { return m_base.addMSecs(m_ms); }
    void advanceSeconds(qint64 seconds) { m_ms += seconds * 1000; }
    const QDateTime &base() const { return m_base; }

private:
    qint64 m_ms = 0;
    QDateTime m_base{QDate(2026, 1, 5), QTime(9, 0), QTimeZone::UTC};
};
