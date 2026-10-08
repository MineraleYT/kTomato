// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrayController.h"

#include "AppSettings.h"
#include "PresetModel.h"
#include "TimerEngine.h"
#include "TrayPresenter.h"

#include <KLocalizedString>
#include <KStatusNotifierItem>

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

#include <cmath>

namespace
{
const QString kIconName = QStringLiteral("io.github.mineraleyt.ktomato");

/// The application icon at several sizes:
/// - When idle (minutes < 0): returns the classic kTomato tomato icon.
/// - When active (minutes >= 0): returns a dynamic chronometer/timer icon with:
///   * Red theme for Work phase
///   * Green theme for Break phase (short and long breaks)
///   * Clockwise sweeping progress ring indicating phase completion
///   * Bold remaining minutes countdown inside the dial (or pie progress / plain hands)
QIcon trayIcon(int minutes, int badgeStyle, double progressRatio, bool isWork, bool isPaused)
{
    if (minutes < 0) {
        return QGuiApplication::windowIcon();
    }

    QIcon result;
    for (const int size : {22, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        if (isPaused) {
            p.setOpacity(0.75);
        }

        const QColor workPrimary(0xef, 0x44, 0x44);      // Vibrant red for Work
        const QColor workTrack(0xef, 0x44, 0x44, 55);
        const QColor breakPrimary(0x22, 0xc5, 0x5e);     // Vibrant emerald green for Break
        const QColor breakTrack(0x22, 0xc5, 0x5e, 55);

        const QColor primary = isWork ? workPrimary : breakPrimary;
        const QColor track = isWork ? workTrack : breakTrack;
        const QColor dialBg(20, 23, 28, 240);            // High contrast dark dial

        const double cx = size / 2.0;

        // 1. Crown / pusher at 12 o'clock
        const double crownW = qMax(3.0, std::round(size * 0.22));
        const double crownH = qMax(1.5, std::round(size * 0.08));
        const double crownY = 0.5;
        const QRectF crownRect(cx - crownW / 2.0, crownY, crownW, crownH);

        p.setPen(Qt::NoPen);
        p.setBrush(primary);
        p.drawRoundedRect(crownRect, 1.0, 1.0);

        // Stem connecting crown to body
        const double stemW = qMax(1.5, std::round(size * 0.09));
        const double stemH = qMax(1.0, std::round(size * 0.05));
        const QRectF stemRect(cx - stemW / 2.0, crownY + crownH, stemW, stemH);
        p.drawRect(stemRect);

        // 2. Circular Dial Body
        const double dialMarginTop = crownY + crownH + stemH;
        const double dialD = size - dialMarginTop - 1.0;
        const double dialY = dialMarginTop + 0.5;
        const double dialX = (size - dialD) / 2.0;
        const QRectF dialRect(dialX, dialY, dialD, dialD);

        // Dial background
        p.setBrush(dialBg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(dialRect);

        const double penWidth = qMax(1.8, std::round(size * 0.11));
        const QRectF arcRect = dialRect.adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);
        const double clampedProgress = qBound(0.0, progressRatio, 1.0);
        const int spanAngle = qRound(360.0 * 16.0 * clampedProgress);

        if (badgeStyle == 1) {
            // Pie style: filled pie inside
            QPen trackPen(track, penWidth, Qt::SolidLine);
            p.setPen(trackPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(arcRect);

            if (clampedProgress > 0.005) {
                p.setPen(Qt::NoPen);
                p.setBrush(primary);
                const QRectF pieRect = arcRect.adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);
                p.drawPie(pieRect, 90 * 16, -spanAngle);
            }
        } else if (badgeStyle == 2) {
            // Plain icon: outer arc + hands
            QPen trackPen(track, penWidth, Qt::SolidLine, Qt::RoundCap);
            p.setPen(trackPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(arcRect);

            if (clampedProgress > 0.005) {
                QPen progressPen(primary, penWidth, Qt::SolidLine, Qt::RoundCap);
                p.setPen(progressPen);
                p.drawArc(arcRect, 90 * 16, -spanAngle);
            }

            // Clock hands
            p.setPen(QPen(Qt::white, qMax(1.2, size * 0.07), Qt::SolidLine, Qt::RoundCap));
            const QPointF center = dialRect.center();
            const double handLen = dialD * 0.28;
            p.drawLine(center, center + QPointF(-handLen * 0.6, -handLen * 0.6));
            p.drawLine(center, center + QPointF(handLen * 0.7, -handLen * 0.7));
        } else {
            // badgeStyle == 0: Remaining minutes
            QPen trackPen(track, penWidth, Qt::SolidLine, Qt::RoundCap);
            p.setPen(trackPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(arcRect);

            if (clampedProgress > 0.005) {
                QPen progressPen(primary, penWidth, Qt::SolidLine, Qt::RoundCap);
                p.setPen(progressPen);
                p.drawArc(arcRect, 90 * 16, -spanAngle);
            }

            if (minutes >= 0) {
                const QString text = QString::number(minutes);
                QFont font = p.font();
                font.setBold(true);
                if (size <= 24) {
                    font.setPixelSize(text.length() >= 2 ? 10 : 11);
                } else if (size <= 32) {
                    font.setPixelSize(text.length() >= 2 ? 13 : 15);
                } else {
                    font.setPixelSize(text.length() >= 2 ? qRound(dialD * 0.44) : qRound(dialD * 0.52));
                }
                p.setFont(font);
                p.setPen(Qt::white);
                const QRectF textRect = dialRect.adjusted(0, size <= 24 ? -0.5 : 0, 0, 0);
                p.drawText(textRect, Qt::AlignCenter, text);
            }
        }

        p.end();
        result.addPixmap(pixmap);
    }
    return result;
}
} // namespace

TrayController::TrayController(TimerEngine *engine, PresetModel *presets, AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_presets(presets)
    , m_settings(settings)
{
    connect(engine, &TimerEngine::stateChanged, this, &TrayController::refresh);
    connect(engine, &TimerEngine::phaseChanged, this, &TrayController::refresh);
    connect(engine, &TimerEngine::remainingChanged, this, &TrayController::refresh);
    connect(presets, &PresetModel::currentChanged, this, &TrayController::refresh);
    connect(presets, &PresetModel::presetChanged, this, &TrayController::refresh);
    // Anything that can change the list of timers; rebuildTimerMenu() skips unchanged lists.
    for (auto signal : {&PresetModel::modified, &PresetModel::currentChanged, &PresetModel::countChanged}) {
        connect(presets, signal, this, &TrayController::rebuildTimerMenu);
    }
    connect(presets, &PresetModel::rowsInserted, this, &TrayController::rebuildTimerMenu);
    connect(presets, &PresetModel::rowsRemoved, this, &TrayController::rebuildTimerMenu);
    connect(presets, &PresetModel::rowsMoved, this, &TrayController::rebuildTimerMenu);
    connect(presets, &PresetModel::modelReset, this, &TrayController::rebuildTimerMenu);
    connect(presets, &PresetModel::dataChanged, this, &TrayController::rebuildTimerMenu);
    connect(settings, &AppSettings::showTrayIconChanged, this, &TrayController::sync);
    connect(settings, &AppSettings::trayAvailableChanged, this, &TrayController::sync);
    connect(settings, &AppSettings::trayBadgeStyleChanged, this, &TrayController::refresh);
    connect(settings, &AppSettings::languageChanged, this, &TrayController::retranslate);
    sync();
}

TrayController::~TrayController() = default;

void TrayController::setWindow(QWindow *window)
{
    m_window = window;
    if (m_item) {
        m_item->setAssociatedWindow(window);
    }
}

void TrayController::sync()
{
    const bool wanted = m_settings->showTrayIcon() && m_settings->trayAvailable();
    if (wanted && !m_item) {
        createItem();
    } else if (!wanted && m_item) {
        destroyItem();
    }
}

void TrayController::createItem()
{
    m_item = std::make_unique<KStatusNotifierItem>(kIconName);
    m_item->setCategory(KStatusNotifierItem::ApplicationStatus);
    // Always Active: a Passive icon would be tucked away in Plasma's hidden-icons area.
    m_item->setStatus(KStatusNotifierItem::Active);

    auto *menu = new QMenu; // the item takes ownership
    m_statusAction = menu->addAction(QString());
    m_statusAction->setEnabled(false);
    menu->addSeparator();
    m_toggleAction = menu->addAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), QString(), this, [this]() {
        m_engine->toggle();
    });
    m_stopAction = menu->addAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), QString(), this, [this]() {
        m_engine->stop();
    });
    m_skipAction = menu->addAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), QString(), this, [this]() {
        m_engine->skip();
    });
    menu->addSeparator();
    m_timerMenu = menu->addMenu(QIcon::fromTheme(QStringLiteral("chronometer")), QString());
    m_timerGroup = new QActionGroup(m_timerMenu);
    m_timerGroup->setExclusive(true);
    menu->addSeparator();
    m_statsAction = menu->addAction(QIcon::fromTheme(QStringLiteral("office-chart-bar")), QString(), this, [this]() {
        Q_EMIT openPageRequested(QStringLiteral("stats"));
    });
    m_settingsAction = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), QString(), this, [this]() {
        Q_EMIT openPageRequested(QStringLiteral("settings"));
    });
    // No separator here: KStatusNotifierItem puts its own before "Restore/Minimize" and "Quit".
    // The clock in the status line is refreshed only when the menu opens, never on a tick.
    connect(menu, &QMenu::aboutToShow, this, [this]() {
        updateStatusLine();
        rebuildTimerMenu();
    });
    m_item->setContextMenu(menu);
    // Adds "Restore/Minimize" and "Quit" below our actions.
    m_item->setStandardActionsEnabled(true);
    connect(m_item.get(), &KStatusNotifierItem::quitRequested, qApp, &QCoreApplication::quit);

    if (m_window) {
        m_item->setAssociatedWindow(m_window);
    }

    retranslate();
}

void TrayController::destroyItem()
{
    m_item.reset(); // also deletes the menu and its actions
}

void TrayController::retranslate()
{
    if (!m_item) {
        return;
    }
    if (m_statsAction) {
        m_statsAction->setText(i18n("Statistics"));
    }
    if (m_settingsAction) {
        m_settingsAction->setText(i18n("Settings"));
    }
    if (m_timerMenu) {
        m_timerMenu->setTitle(i18n("Timer"));
    }
    if (m_stopAction) {
        m_stopAction->setText(i18n("Stop"));
    }
    if (m_skipAction) {
        m_skipAction->setText(i18n("Skip"));
    }
    // Forces the tooltip (and the toggle action below) to be rebuilt in the new language.
    m_iconKey.clear();
    m_toolTipTitle.clear();
    m_toolTipText.clear();
    m_menuKey.clear();
    m_timerMenuKey.clear();
    rebuildTimerMenu();
    refresh();
}

void TrayController::updateStatusLine()
{
    if (m_statusAction) {
        const QString text = menuStatusLine(m_engine->state(), m_engine->phase(), m_engine->remainingSeconds(),
                                            m_presets->currentPreset().name);
        if (m_statusAction->text() != text) {
            m_statusAction->setText(text);
        }
    }
}

void TrayController::rebuildTimerMenu()
{
    if (!m_item || !m_timerMenu || !m_timerGroup) {
        return;
    }
    const QList<TimerPreset> &list = m_presets->presets();
    // The key covers what the menu entries are made of, NOT which timer is current: choosing another
    // timer must only move the check mark. Rebuilding the entries on every choice gave the desktop a
    // brand-new menu while it still showed the old one, and it ended up with two timers ticked.
    QString key;
    for (int row = 0; row < list.size(); ++row) {
        key += list.at(row).uuid + QLatin1Char('\t') + list.at(row).name + QLatin1Char('\t')
            + m_presets->data(m_presets->index(row), PresetModel::IconNameRole).toString() + QLatin1Char('\n');
    }
    if (key != m_timerMenuKey) {
        m_timerMenuKey = key;
        m_timerMenu->clear(); // deletes the old actions, which leaves the group
        for (int row = 0; row < list.size(); ++row) {
            const TimerPreset &preset = list.at(row);
            const QString iconName = m_presets->data(m_presets->index(row), PresetModel::IconNameRole).toString();
            QAction *action = m_timerMenu->addAction(iconName.isEmpty() ? QIcon() : QIcon::fromTheme(iconName), preset.name);
            action->setCheckable(true);
            action->setData(preset.uuid);
            m_timerGroup->addAction(action);
            const QString uuid = preset.uuid;
            connect(action, &QAction::triggered, this, [this, uuid]() {
                m_presets->setCurrentUuid(uuid);
                // If the choice was refused the clicked entry must not stay ticked.
                syncTimerChecks();
            });
        }
    }
    syncTimerChecks();
}

void TrayController::syncTimerChecks()
{
    if (!m_timerGroup) {
        return;
    }
    // The group is exclusive: ticking the current timer unticks every other one, and only the
    // actions whose state really changes are announced to the desktop.
    const QString current = m_presets->currentUuid();
    const QList<QAction *> actions = m_timerGroup->actions();
    for (QAction *action : actions) {
        if (action->data().toString() == current && !action->isChecked()) {
            action->setChecked(true);
            return;
        }
    }
}

void TrayController::refresh()
{
    if (!m_item) {
        return;
    }

    const TrayStatus status = describeTray(m_engine->state(), m_engine->phase(), m_engine->remainingSeconds(),
                                           m_presets->currentPreset().name);
    if (m_item->title() != status.title) {
        m_item->setTitle(status.title);
    }

    const int badgeStyle = m_settings ? m_settings->trayBadgeStyle() : 0;
    const bool isWork = m_engine->phase() == TimerEngine::Phase::Work;
    const bool isPaused = m_engine->isPaused();
    // The ring is drawn in 1/16 degree steps; 1/200 of a turn is finer than any tray size shows.
    const int progressStep = status.badgeMinutes >= 0 ? qRound(m_engine->progress() * 200.0) : 0;

    // Repainting and sending pixmaps over D-Bus is costly: do it only when the picture changes.
    const QString iconKey = QStringLiteral("%1/%2/%3/%4/%5")
                                .arg(status.badgeMinutes)
                                .arg(badgeStyle)
                                .arg(progressStep)
                                .arg(isWork ? 1 : 0)
                                .arg(isPaused ? 1 : 0);
    bool iconChanged = false;
    if (iconKey != m_iconKey) {
        m_iconKey = iconKey;
        m_badgeMinutes = status.badgeMinutes;
        const QIcon icon = trayIcon(m_badgeMinutes, badgeStyle, progressStep / 200.0, isWork, isPaused);
        m_item->setIconByPixmap(icon);
        m_tooltipIcon = icon;
        iconChanged = true;
    }
    if (iconChanged || status.toolTipTitle != m_toolTipTitle || status.toolTipText != m_toolTipText) {
        m_toolTipTitle = status.toolTipTitle;
        m_toolTipText = status.toolTipText;
        m_item->setToolTip(m_tooltipIcon, status.toolTipTitle, status.toolTipText);
    }

    if (m_toggleAction) {
        const bool idle = m_engine->state() == TimerEngine::State::Idle;
        const bool paused = m_engine->isPaused();
        QString text;
        if (paused) {
            text = i18n("Resume");
        } else if (idle) {
            text = m_engine->phase() == TimerEngine::Phase::Work ? i18n("Start") : i18n("Start break");
        } else {
            text = i18n("Pause");
        }
        if (m_toggleAction->text() != text) {
            m_toggleAction->setText(text);
            m_toggleAction->setIcon(QIcon::fromTheme(idle || paused ? QStringLiteral("media-playback-start")
                                                                     : QStringLiteral("media-playback-pause")));
        }
    }

    // Everything below changes only with the state, the phase or the timer, not with each tick.
    const bool active = m_engine->isActive();
    // While idle the clock is the full phase length and can change with the timer; while a phase
    // runs it ticks, so it stays out of the key.
    const QString stableKey = QStringLiteral("%1/%2/%3/%4/%5")
                                  .arg(int(m_engine->state()))
                                  .arg(int(m_engine->phase()))
                                  .arg(m_presets->currentPreset().name, m_presets->currentUuid())
                                  .arg(active ? -1 : m_engine->remainingSeconds());
    if (stableKey != m_menuKey) {
        m_menuKey = stableKey;
        updateStatusLine();
        if (m_stopAction) {
            m_stopAction->setVisible(active);
        }
        if (m_skipAction) {
            m_skipAction->setVisible(active);
        }
        if (m_timerMenu) {
            m_timerMenu->setEnabled(!active);
        }
    }
}
