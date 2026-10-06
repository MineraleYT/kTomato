// SPDX-License-Identifier: GPL-3.0-or-later
#include "PhaseNotifier.h"

#include "NotificationInhibitor.h"
#include "PresetModel.h"
#include "AppSettings.h"

#include <KLocalizedString>
#include <KNotification>
#include <KNotificationReplyAction>

#include <algorithm>

namespace
{
/// "25 minutes", or "30 seconds" for phases shorter than a minute.
QString durationText(int seconds)
{
    if (seconds < 60) {
        return i18np("%1 second", "%1 seconds", std::max(0, seconds));
    }
    return i18np("%1 minute", "%1 minutes", (seconds + 30) / 60);
}
} // namespace

PhaseNotifier::PhaseNotifier(TimerEngine *engine, PresetModel *presets, NotificationInhibitor *inhibitor, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_presets(presets)
    , m_inhibitor(inhibitor)
    , m_settings(settings)
{
    connect(engine, &TimerEngine::phaseFinished, this, &PhaseNotifier::onPhaseFinished);
    connect(engine, &TimerEngine::oneMinuteRemaining, this, &PhaseNotifier::onOneMinuteRemaining);
    connect(engine, &TimerEngine::phaseStarted, this, [this]() {
        ++m_phaseSerial;
    });
}

void PhaseNotifier::setLastWorkSessionIdProvider(std::function<qint64()> provider)
{
    m_lastWorkSessionId = std::move(provider);
}

void PhaseNotifier::onOneMinuteRemaining(TimerEngine::Phase phase)
{
    Q_UNUSED(phase)
    if (m_settings && m_settings->preAlarmEnabled()) {
        m_settings->playSoundPreview(m_settings->preAlarmSound(), m_settings->soundVolume());
    }
}

void PhaseNotifier::onPhaseFinished(TimerEngine::Phase finished, TimerEngine::Phase next)
{
    const TimerPreset preset = m_presets->currentPreset();
    const bool popup = preset.boolOption(PresetOption::NotifyOnEnd);
    const bool sound = preset.boolOption(PresetOption::SoundOnEnd);
    if (!popup && !sound) {
        return;
    }

    // Captured now: phaseFinished() comes after the transition, so this is the real state, and
    // the work session that just ended has been recorded already.
    const bool autoStarted = m_engine->isRunning();
    const quint64 serial = m_phaseSerial;
    const qint64 sessionId = (finished == TimerEngine::Phase::Work && m_lastWorkSessionId) ? m_lastWorkSessionId() : 0;

    // The silencing of a work phase is released the moment the phase ends. Wait until the
    // desktop confirms it, or this notification would be swallowed too.
    m_inhibitor->whenReleased(this, [this, finished, next, popup, sound, preset, autoStarted, serial, sessionId]() {
        if (sound && m_settings) {
            m_settings->playSoundPreview(m_settings->soundTheme(), m_settings->soundVolume());
        }
        if (!popup) {
            return;
        }

        QString title;
        QString text;
        if (finished == TimerEngine::Phase::Work) {
            title = i18n("Work session finished");
            if (next == TimerEngine::Phase::LongBreak) {
                text = autoStarted ? i18n("Long break started: %1.", durationText(preset.longBreakSeconds))
                                   : i18n("Time for a long break: %1.", durationText(preset.longBreakSeconds));
            } else if (next == TimerEngine::Phase::ShortBreak) {
                text = autoStarted ? i18n("Short break started: %1.", durationText(preset.shortBreakSeconds))
                                   : i18n("Time for a short break: %1.", durationText(preset.shortBreakSeconds));
            } else {
                // No break configured.
                text = autoStarted ? i18n("The next work session has started.") : i18n("Time to start the next work session.");
            }
        } else {
            title = i18n("Break is over");
            text = autoStarted ? i18n("Work session started.") : i18n("Time to get back to work.");
        }

        // Sound is played by kTomato so the selected volume applies consistently;
        // this notification event only requests the visible popup.
        auto *notification = new KNotification(QStringLiteral("phaseEndedSilent"), KNotification::CloseOnTimeout, this);
        notification->setComponentName(QStringLiteral("ktomato"));
        notification->setTitle(title);
        notification->setText(text);
        notification->setIconName(QStringLiteral("io.github.mineraleyt.ktomato"));

        const bool offerNote = finished == TimerEngine::Phase::Work && sessionId > 0 && m_settings && m_settings->promptTaskNote();
        if (offerNote) {
            auto replyAction = std::make_unique<KNotificationReplyAction>(i18n("Log Task"));
            replyAction->setPlaceholderText(i18n("What did you accomplish?"));
            replyAction->setSubmitButtonText(i18n("Save"));
            replyAction->setSubmitButtonIconName(QStringLiteral("document-save"));
            replyAction->setFallbackBehavior(KNotificationReplyAction::FallbackBehavior::UseRegularAction);
            connect(replyAction.get(), &KNotificationReplyAction::replied, this, [this, sessionId](const QString &note) {
                if (!note.trimmed().isEmpty()) {
                    Q_EMIT taskNoteSubmitted(sessionId, note.trimmed());
                }
            });
            connect(replyAction.get(), &KNotificationReplyAction::activated, this, [this, preset, sessionId]() {
                Q_EMIT taskNotePromptRequested(preset.name, sessionId);
            });
            notification->setReplyAction(std::move(replyAction));
        }

        // The timer is still where this notification left it: no phase started since, and the
        // phase that was started (or is pending) is still the current one.
        const auto unchanged = [this, serial, next, autoStarted]() {
            return m_phaseSerial == serial && m_engine->phase() == next
                && (autoStarted ? m_engine->isActive() : m_engine->isIdle());
        };
        const QString skipLabel = (next == TimerEngine::Phase::Work) ? i18n("Skip") : i18n("Skip Break");

        if (autoStarted) {
            auto *pauseAction = notification->addAction(next == TimerEngine::Phase::Work ? i18n("Pause Work") : i18n("Pause Break"));
            connect(pauseAction, &KNotificationAction::activated, this, [this, unchanged]() {
                if (unchanged() && m_engine->isRunning()) {
                    m_engine->pause();
                }
            });
        } else {
            QString startLabel;
            switch (next) {
            case TimerEngine::Phase::LongBreak:
                startLabel = i18n("Start Long Break");
                break;
            case TimerEngine::Phase::ShortBreak:
                startLabel = i18n("Start Short Break");
                break;
            case TimerEngine::Phase::Work:
                startLabel = i18n("Start Work");
                break;
            }
            auto *startAction = notification->addAction(startLabel);
            connect(startAction, &KNotificationAction::activated, this, [this, unchanged]() {
                if (unchanged()) {
                    m_engine->start();
                }
            });
        }
        auto *skipAction = notification->addAction(skipLabel);
        connect(skipAction, &KNotificationAction::activated, this, [this, unchanged]() {
            if (unchanged()) {
                m_engine->skip();
            }
        });

        auto *defaultAction = notification->addDefaultAction(i18n("View"));
        connect(defaultAction, &KNotificationAction::activated, this, [this, offerNote, preset, sessionId]() {
            if (offerNote && m_settings->promptTaskNote()) {
                Q_EMIT taskNotePromptRequested(preset.name, sessionId);
            } else {
                Q_EMIT showWindowRequested();
            }
        });

        notification->sendEvent();
    });
}
