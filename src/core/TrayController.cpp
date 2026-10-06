// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrayController.h"

#include "AppSettings.h"
#include "PresetModel.h"
#include "TimerEngine.h"
#include "TrayPresenter.h"

#include <KLocalizedString>
#include <KStatusNotifierItem>

#include <QAction>
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
    m_toggleAction = menu->addAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), QString(), this, [this]() {
        m_engine->toggle();
    });
    m_stopAction = menu->addAction(QIcon::fromTheme(QStringLiteral("media-playback-stop")), QString(), this, [this]() {
        m_engine->stop();
    });
    m_skipAction = menu->addAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), QString(), this, [this]() {
        m_engine->skip();
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
    refresh();
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
    if (m_stopAction) {
        m_stopAction->setEnabled(m_engine->isActive());
    }
    if (m_skipAction) {
        m_skipAction->setEnabled(m_engine->isActive());
    }
}
