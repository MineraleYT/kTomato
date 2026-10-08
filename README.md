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

**kTomato** is an independent, native Pomodoro and interval timer built specifically for **KDE Plasma 6** and Linux desktops. Following the **KDE Breeze Human Interface Guidelines (HIG)**, it blends seamlessly into the desktop while keeping your data 100% private, local, and distraction-free.

---

## ✨ Features

- ⏱️ **Flexible Focus & Breaks**: Work, short break, and long break intervals with configurable durations and custom cycle counts.
- ⚡ **Preset Library**: Ready-to-use routines (*Pomodoro*, *Deep Work*, *Study*, *Quick Sprint*) with support for custom names, categories, and 11 symbolic icons.
- 🌐 **Full Internationalization (i18n)**:
  - Translated into **English**, **Italian**, **German**, **Spanish**, and **French**.
  - Automatic OS locale detection with English fallback.
  - Live on-the-fly language switching from both the Welcome walkthrough and Settings.
- 🔕 **Distraction-Free Environment**:
  - Automatically silences desktop notifications during focus phases (via `org.freedesktop.Notifications`).
  - Screen-sleep inhibition during active sessions (via `org.freedesktop.ScreenSaver`).
  - Optional automatic pause when the screen is locked, with a resume prompt when you come back.
  - Ambient background sounds: clock ticking, rain, and white noise (played locally through PipeWire or ALSA).
- 📊 **Productivity Statistics & Insights**:
  - Daily, weekly, monthly, and yearly breakdowns.
  - Distribution breakdown by timer preset and category.
  - Daily goal tracking with streak counters and weekend streak protection.
  - Export session logs to CSV anytime.
- 📝 **Task Logging**:
  - Optional prompt upon completing a work session to record what you accomplished.
  - Log tasks directly from the Plasma notification (inline reply or action) or via the in-app dialog with quick suggestions.
  - Task notes are stored with session records and exported in CSV reports.
- 📅 **Calendar Events (optional)**:
  - Log in with **Nextcloud** (authorized in your browser, no password to copy) or connect any **CalDAV** calendar from Settings → Calendar.
  - Every finished work session becomes an event with the timer name, its category and the real start and end times. The task note is added only if you turn that on.
  - Off by default. Failed sends are retried while kTomato runs. A calendar *subscription* link ending in `.ics` is read-only and cannot receive events: use the CalDAV address.
- 🏃 **KRunner Search Integration**:
  - Control your timer directly from Plasma's launcher (`Alt+Space` or `Meta`).
  - Search `pomodoro` or `ktomato` to view current remaining time, pause, resume, skip, or stop.
  - Quick sub-commands: `pomodoro start`, `pause`, `resume`, `stop`, `skip`, start a preset by name (`pomodoro deep`), or start ad-hoc sessions like `pomodoro 25`.
- 🎛️ **Deep Desktop Integration**:
  - Dynamic panel chronometer with real-time circular progress and phase indicator (work/break).
  - Native KDE notifications with interactive actions to immediately start, pause, or skip intervals.
  - Start at login and minimize-to-tray support.
  - Breeze-style interface with a selectable color scheme (Breeze Dark by default, Breeze Light, or the system scheme) and keyboard shortcuts: `Space` starts or pauses the timer, `Ctrl+1`…`Ctrl+5` switch pages, `Ctrl+,` opens Settings, and `Ctrl+S` / `Esc` save or cancel in the timer editor.
  - An animated Welcome tour for the first start.
- 🤖 **D-Bus Automation**: Full CLI and script control interface to bind global shortcuts or automate workflows.
- 🔒 **Offline & Private**: Everything is stored in a local SQLite database (`~/.local/share/ktomato/ktomato.db`). Includes one-click database backup and restore. kTomato sends nothing on its own. Network access happens only in two optional cases: the update check you start yourself from the About page (it asks the GitHub releases API for the latest version number), and the calendar sync, if you turn it on in Settings (it sends the timer name, category and times of each finished work session to the calendar server you choose).
- 🩺 **Diagnostics**: Settings → Troubleshooting can copy or export a report with the version, environment and recent log lines (home directory and user name are redacted) to attach to bug reports.

---

## 📥 Installation

### Option 1: Flatpak (Recommended)

GitHub releases provide a standalone, pre-built x86_64 Flatpak bundle:

1. Download `ktomato-v1.0.1-x86_64.flatpak` from [Releases](https://github.com/MineraleYT/kTomato/releases/latest).
2. Install with:
   ```sh
   flatpak install --user ./ktomato-v1.0.1-x86_64.flatpak
   ```
3. Launch:
   ```sh
   flatpak run io.github.mineraleyt.ktomato
   ```

*Note: Flatpak downloads the standard KDE 6.11 runtime from Flathub during installation if not already present.*

#### Build Flatpak locally

```sh
flatpak-builder --user --install --install-deps-from=flathub --force-clean \
    "$HOME/.cache/ktomato-flatpak-build" io.github.mineraleyt.ktomato.yml
```

---

### Option 2: Build from Source

#### Requirements

- **C++17** compiler (GCC 11+ or Clang 14+)
- **CMake** 3.20+ and **Ninja**
- **Qt 6.6+** (Core, Gui, Widgets, Qml, Quick, QuickControls2, Sql, DBus, Network)
- **KDE Frameworks 6.8+** (Kirigami, I18n, CoreAddons, ColorScheme, Notifications, Config, StatusNotifierItem)
- **Gettext** (`msgfmt`, `xgettext`, `msgmerge`)

#### Install Build Dependencies

##### Fedora
```sh
sudo dnf install cmake ninja-build gcc-c++ gettext \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-extra-cmake-modules kf6-kirigami-devel kf6-ki18n-devel \
    kf6-kcoreaddons-devel kf6-kcolorscheme-devel kf6-knotifications-devel \
    kf6-kconfig-devel kf6-kstatusnotifieritem-devel kf6-qqc2-desktop-style
```

##### Ubuntu 26.04 or newer
```sh
sudo apt install cmake ninja-build g++ gettext \
    qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
    extra-cmake-modules libkirigami-dev qml6-module-org-kde-desktop libkf6i18n-dev \
    libkf6coreaddons-dev libkf6colorscheme-dev libkf6notifications-dev \
    libkf6config-dev libkf6statusnotifieritem-dev
```

##### Arch Linux
```sh
sudo pacman -S cmake ninja gcc gettext qt6-base qt6-declarative \
    extra-cmake-modules qqc2-desktop-style kirigami ki18n kcoreaddons \
    kcolorscheme knotifications kconfig kstatusnotifieritem
```

##### openSUSE Tumbleweed
```sh
sudo zypper install cmake ninja gcc-c++ gettext-tools \
    qt6-base-devel qt6-declarative-devel extra-cmake-modules \
    kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
    kf6-kcolorscheme-devel kf6-knotifications-devel kf6-kconfig-devel \
    kf6-kstatusnotifieritem-devel
```

#### Compile and Install

```sh
git clone https://github.com/MineraleYT/kTomato.git
cd kTomato

cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local"

cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

To install system-wide, set `-DCMAKE_INSTALL_PREFIX=/usr` and run the install command with `sudo`.

---

## 🤖 D-Bus Automation & Shortcuts

kTomato exposes a complete native D-Bus interface at:
- **Service**: `io.github.mineraleyt.ktomato`
- **Path**: `/Timer`
- **Interface**: `io.github.mineraleyt.ktomato.Timer`

### Available Commands

| Command | Action |
|:---|:---|
| `qdbus io.github.mineraleyt.ktomato /Timer toggle` | Toggle timer state (start or pause) |
| `qdbus io.github.mineraleyt.ktomato /Timer start` | Start or resume the active timer |
| `qdbus io.github.mineraleyt.ktomato /Timer pause` | Pause the active timer |
| `qdbus io.github.mineraleyt.ktomato /Timer stop` | Stop and reset the timer |
| `qdbus io.github.mineraleyt.ktomato /Timer skip` | Skip to the next phase |
| `qdbus io.github.mineraleyt.ktomato /Timer status` | Print the current status as `state:phase:seconds` (see below) |

`status` returns a plain string of three colon-separated fields, for example `running:work:1342`:
- **state**: `idle`, `running` or `paused`
- **phase**: `work`, `short_break` or `long_break` (when idle, the phase that starts next)
- **seconds**: remaining time of the phase, in whole seconds

> **Note**: Depending on your distribution, `qdbus` may be installed as `qdbus6` or `qdbus-qt6`. If kTomato is not running, calling the service starts it hidden in the system tray (D-Bus activation).

> **Tip**: You can bind any global shortcut key in **KDE System Settings → Shortcuts → Add Custom Command** to easily control your focus timer from your keyboard.

---

## 🏃 KRunner Integration

kTomato includes a native D-Bus runner plugin for KDE Plasma 6. Press <kbd>Alt</kbd> + <kbd>Space</kbd> or <kbd>Meta</kbd> and type:
- `pomodoro`, `ktomato`, `tomato` or `timer`: inspect current status, remaining duration, and quick actions.
- `pomodoro start`: start or resume the active timer.
- `pomodoro pause` / `pomodoro resume`: pause or continue the countdown.
- `pomodoro stop`: reset and stop the session.
- `pomodoro skip`: advance to the next phase.
- `pomodoro <preset name>` (or part of it): switch to that preset and start it.
- `pomodoro 25` (or any minutes 1–180): start a one-off focus session of that length; your preset's own duration stays unchanged.

While a timer runs, the status result also shows action buttons (hover or select it, or press <kbd>Shift</kbd>+<kbd>Enter</kbd>): **Pause**/**Resume**, **Skip** and **Stop**. Each button runs its own command; activating the result itself pauses or resumes.

---

## 🌍 Contributing & Localization

Contributions, bug reports, and translations are very welcome!

### Testing
Run the complete automated test suite (CTest):
```sh
ctest --test-dir build --output-on-failure
```

### Translating
kTomato uses standard KDE Gettext (`po/`) catalogs:
1. Extract current strings:
   ```sh
   bash po/Messages.sh
   ```
2. Create or update your language file in `po/<lang_code>/ktomato.po`.
3. Validate translations with:
   ```sh
   msgfmt -c -v -o /dev/null po/<lang_code>/ktomato.po
   ```

---

## 📄 License

- Application code is licensed under the [GNU General Public License v3.0 or later (GPL-3.0-or-later)](LICENSE).
- Documentation and media are licensed under [Creative Commons Attribution-ShareAlike 4.0 International (CC-BY-SA-4.0)](https://creativecommons.org/licenses/by-sa/4.0/).
- AppStream metadata is licensed under [CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/).

*kTomato is an independent community project and is not affiliated with the KDE e.V. or official KDE releases.*
