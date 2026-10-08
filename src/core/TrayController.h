// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIcon>
#include <QObject>
#include <QPointer>

#include <memory>

class AppSettings;
class KStatusNotifierItem;
class PresetModel;
class QActionGroup;
class QAction;
class QMenu;
class QWindow;
class TimerEngine;

/**
 * The system tray icon: shows the phase and the remaining time (tooltip, and the minutes
 * as a badge on the icon), offers start/pause, stop and skip in its menu, and shows or
 * hides the window when clicked. Created and removed as the "Show tray icon" setting
 * changes.
 */
class TrayController : public QObject
{
    Q_OBJECT

public:
    TrayController(TimerEngine *engine, PresetModel *presets, AppSettings *settings, QObject *parent = nullptr);
    ~TrayController() override;

    /// The window the icon shows and hides when clicked.
    void setWindow(QWindow *window);

    bool isVisible() const { return m_item != nullptr; }

Q_SIGNALS:
    /// A menu entry asks for the window on a page: "stats" or "settings".
    void openPageRequested(const QString &page);

private:
    void sync();
    void createItem();
    void destroyItem();
    void refresh();
    void retranslate();
    void updateStatusLine();
    void rebuildTimerMenu();
    void syncTimerChecks();

    TimerEngine *m_engine;
    PresetModel *m_presets;
    AppSettings *m_settings;
    QPointer<QWindow> m_window;

    std::unique_ptr<KStatusNotifierItem> m_item;
    // The actions belong to the item's menu, which the item owns.
    QPointer<QAction> m_toggleAction;
    QPointer<QAction> m_stopAction;
    QPointer<QAction> m_skipAction;
    QPointer<QAction> m_statusAction;
    QPointer<QAction> m_statsAction;
    QPointer<QAction> m_settingsAction;
    QPointer<QMenu> m_timerMenu;
    QPointer<QActionGroup> m_timerGroup;
    QString m_timerMenuKey;    ///< What the Timer submenu currently lists, to skip no-op rebuilds.
    QString m_menuKey;         ///< State the status line and the visibility were last set for.
    int m_badgeMinutes = -1;   ///< Minutes the badge shows (-1 = none), for describeTray() callers.
    QString m_iconKey;         ///< Everything the drawn icon depends on; empty forces the next draw.
    QString m_toolTipTitle;    ///< The tooltip last sent, so unchanged text is not resent.
    QString m_toolTipText;
    QIcon m_tooltipIcon;       ///< The icon last sent to the tray, reused in the tooltip.
};
