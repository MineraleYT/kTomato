<!--
  SPDX-FileCopyrightText: 2026 kTomato contributors
  SPDX-License-Identifier: CC-BY-SA-4.0
-->

<div align="center">

  <img src="data/icons/hicolor/scalable/apps/io.github.mineraleyt.ktomato.svg" alt="kTomato logo" width="128" height="128" />

  # kTomato

  **A modern, distraction-free Pomodoro timer designed for KDE Plasma 6.**

  [![CI](https://github.com/MineraleYT/kTomato/actions/workflows/ci.yml/badge.svg)](https://github.com/MineraleYT/kTomato/actions/workflows/ci.yml)
  [![Release Flatpak](https://github.com/MineraleYT/kTomato/actions/workflows/release-flatpak.yml/badge.svg)](https://github.com/MineraleYT/kTomato/actions/workflows/release-flatpak.yml)
  [![Latest Release](https://img.shields.io/github/v/release/MineraleYT/kTomato?color=e05d44&logo=github)](https://github.com/MineraleYT/kTomato/releases/latest)
  [![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
  [![KDE Plasma 6](https://img.shields.io/badge/KDE-Plasma%206-1d99f3?logo=kde)](https://kde.org)
  [![Qt 6](https://img.shields.io/badge/Qt-6.6+-41cd52?logo=qt)](https://www.qt.io)

</div>

---

## 🍅 About kTomato

**kTomato** is an independent, native Pomodoro and interval timer built specifically for **KDE Plasma 6** and Linux desktops. It follows the **KDE Breeze Human Interface Guidelines**, blends into the desktop and keeps your data local and under your control.

---

## ✨ Features

- ⏱️ **Flexible focus and breaks**: work, short and long breaks with configurable durations, cycles and auto-start modes, plus ready-made timers (*Pomodoro*, *Deep Work*, *Study*, *Quick Sprint*) and your own.
- 🔕 **Distraction-free**: silences notifications and keeps the screen awake while you work, can pause when the screen locks, and plays optional ambient sounds.
- 📊 **Statistics**: day, week, month and year views, daily goal and streaks, breakdown by timer, category and task, CSV export.
- 📝 **Task notes**: jot down what you did when a work session ends, from the notification or an in-app prompt.
- 📅 **Calendar events (optional)**: log in with Nextcloud, or use any CalDAV calendar, and every finished work session becomes an event.
- 🎛️ **Desktop integration**: a tray icon with a live countdown and a menu to control the timer, native notifications with actions, KRunner commands, start at login, and a D-Bus interface for scripts and shortcuts.
- 🌐 **Five languages**: English, Italian, German, Spanish and French, switchable on the fly.
- 🔒 **Private**: everything lives in a local SQLite database, with one-click backup and restore. kTomato sends nothing on its own; the only network use is the update check you start and the calendar sync you enable.

See the [user guide](docs/USAGE.md) for the details of each feature.

---

## 📥 Installation

### Flatpak (recommended)

Releases provide a pre-built x86_64 Flatpak bundle:

1. Download `ktomato-v1.0.1-x86_64.flatpak` from [Releases](https://github.com/MineraleYT/kTomato/releases/latest).
2. Install and run it:
   ```sh
   flatpak install --user ./ktomato-v1.0.1-x86_64.flatpak
   flatpak run io.github.mineraleyt.ktomato
   ```

*Flatpak downloads the KDE 6.11 runtime from Flathub the first time, if it is not installed yet.*

### From source

kTomato needs Qt 6.6+ and KDE Frameworks 6.8+. Dependencies for Fedora, Ubuntu, Arch and openSUSE, build and install steps, and a local Flatpak build are in the [development guide](docs/DEVELOPMENT.md#building-from-source).

---

## 🚀 Quick start

- **First start:** a short animated tour lets you pick the language and a daily goal.
- **From the tray:** click the tomato icon for a menu with the status, **Start/Pause**, **Stop**, **Skip**, a **Timer** submenu to switch timers, and shortcuts to Statistics and Settings.
- **From KRunner** (`Alt+Space`): type `pomodoro 25` to start a 25-minute session, or `pomodoro` to see the status and controls.
- **From a script:** `qdbus io.github.mineraleyt.ktomato /Timer toggle`, handy for a global shortcut.

---

## 📚 Documentation

| Document | What it covers |
|:---|:---|
| [User guide](docs/USAGE.md) | Timers, the tray menu, shortcuts, statistics, task notes, calendar sync, KRunner, D-Bus, data and privacy |
| [Development guide](docs/DEVELOPMENT.md) | Building, testing, translating, project layout and the release checklist |

---

## 🤝 Contributing

Bug reports, ideas and translations are very welcome. Settings → Troubleshooting can copy a diagnostics report to attach to an issue (your home directory and user name are redacted). How to build, run the tests and add a translation is in the [development guide](docs/DEVELOPMENT.md).

---

## 📄 License

- Application code is licensed under the [GNU General Public License v3.0 or later (GPL-3.0-or-later)](LICENSE).
- Documentation and media are licensed under [Creative Commons Attribution-ShareAlike 4.0 International (CC-BY-SA-4.0)](https://creativecommons.org/licenses/by-sa/4.0/).
- AppStream metadata is licensed under [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/).

*kTomato is an independent community project and is not affiliated with the KDE e.V. or official KDE releases.*
