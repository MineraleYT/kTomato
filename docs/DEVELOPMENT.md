<!--
  SPDX-FileCopyrightText: 2026 kTomato contributors
  SPDX-License-Identifier: CC-BY-SA-4.0
-->

# kTomato development guide

How to build kTomato, run its tests, add a translation and publish a release. For using the application see the [user guide](USAGE.md).

**Contents**

1. [Building from source](#building-from-source)
2. [Building the Flatpak](#building-the-flatpak)
3. [Tests](#tests)
4. [Project layout](#project-layout)
5. [Translating](#translating)
6. [Releasing](#releasing)

---

## Building from source

### Requirements

- **C++17** compiler (GCC 11+ or Clang 14+)
- **CMake** 3.20+ and **Ninja**
- **Qt 6.6+** (Core, Gui, Widgets, Qml, Quick, QuickControls2, Sql, DBus, Network)
- **KDE Frameworks 6.8+** (Kirigami, I18n, CoreAddons, ColorScheme, Notifications, Config, StatusNotifierItem)
- **Gettext** (`msgfmt`, `xgettext`, `msgmerge`)

### Build dependencies

#### Fedora
```sh
sudo dnf install cmake ninja-build gcc-c++ gettext \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-extra-cmake-modules kf6-kirigami-devel kf6-ki18n-devel \
    kf6-kcoreaddons-devel kf6-kcolorscheme-devel kf6-knotifications-devel \
    kf6-kconfig-devel kf6-kstatusnotifieritem-devel kf6-qqc2-desktop-style
```

#### Ubuntu 26.04 or newer
```sh
sudo apt install cmake ninja-build g++ gettext \
    qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
    extra-cmake-modules libkirigami-dev qml6-module-org-kde-desktop libkf6i18n-dev \
    libkf6coreaddons-dev libkf6colorscheme-dev libkf6notifications-dev \
    libkf6config-dev libkf6statusnotifieritem-dev
```

#### Arch Linux
```sh
sudo pacman -S cmake ninja gcc gettext qt6-base qt6-declarative \
    extra-cmake-modules qqc2-desktop-style kirigami ki18n kcoreaddons \
    kcolorscheme knotifications kconfig kstatusnotifieritem
```

#### openSUSE Tumbleweed
```sh
sudo zypper install cmake ninja gcc-c++ gettext-tools \
    qt6-base-devel qt6-declarative-devel extra-cmake-modules \
    kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
    kf6-kcolorscheme-devel kf6-knotifications-devel kf6-kconfig-devel \
    kf6-kstatusnotifieritem-devel
```

### Compile and install

```sh
git clone https://github.com/MineraleYT/kTomato.git
cd kTomato

cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
    -DBUILD_TESTING=ON

cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

For a system-wide install use `-DCMAKE_INSTALL_PREFIX=/usr` and run the install step with `sudo`.

The install step also puts the KRunner plugin, the D-Bus activation file, the notification definitions and the translations in place. A running kTomato keeps the old program in memory: quit it and start it again after installing, and restart KRunner (`kquitapp6 krunner`) when the runner definition changed.

---

## Building the Flatpak

```sh
flatpak-builder --user --install --install-deps-from=flathub --force-clean \
    "$HOME/.cache/ktomato-flatpak-build" io.github.mineraleyt.ktomato.yml
```

The sandbox permissions are in the `finish-args` of `io.github.mineraleyt.ktomato.yml`.

---

## Tests

```sh
ctest --test-dir build --output-on-failure
```

The suite covers the timer engine, the database and statistics, the platform services (D-Bus, inhibitors, autostart, KRunner), the calendar client against an in-process fake server, and smoke runs of every QML page.

The continuous-integration server has no display and no session bus. To get the same conditions locally, run the tests in a clean environment:

```sh
env -i PATH=/usr/bin:/bin HOME=/tmp/ktomato-test-home \
    ctest --test-dir build --output-on-failure
```

A test that builds a GUI application sets `QT_QPA_PLATFORM=offscreen` for itself (see `tests/CMakeLists.txt`); tests that need D-Bus start a private daemon and are written to work without a session bus. The build is expected to be free of compiler warnings.

---

## Project layout

| Path | Contents |
|:---|:---|
| `src/core/` | Timer engine, presets, settings, notifications, tray, calendar sync, update checker, diagnostics |
| `src/db/` | SQLite access: schema and migrations, presets, sessions, the recorder that stores finished phases |
| `src/stats/` | Statistics aggregation, the model behind the Statistics page, CSV export |
| `src/platform/` | Desktop services: D-Bus API, KRunner, inhibitors, autostart, screen lock, single instance, signals |
| `src/qml/` | The interface (Kirigami): `Main.qml`, `pages/`, `components/` |
| `data/` | Desktop entries, KRunner and D-Bus activation files, notification definitions, AppStream metadata, icon |
| `po/` | Translation template (`ktomato.pot`) and catalogs (`it`, `de`, `es`, `fr`) |
| `tests/` | The test suite |
| `.github/workflows/` | CI and release workflows |

Conventions worth knowing: user-visible strings go through `i18n()` (never `tr()`), with `i18np()` for anything that can be plural; QML uses only `Kirigami.Theme` colours and `Kirigami.Units` sizes.

---

## Translating

kTomato uses standard KDE Gettext catalogs in `po/`:

1. Extract the current strings (this regenerates `po/ktomato.pot`):
   ```sh
   bash po/Messages.sh
   ```
2. Create `po/<lang>/ktomato.po` for a new language, or update an existing one:
   ```sh
   msgmerge --update --backup=none po/<lang>/ktomato.po po/ktomato.pot
   ```
3. Translate the new and *fuzzy* entries and remove the `fuzzy` flags. Keep `%1`, `%2` … placeholders and plural forms intact.
4. Validate:
   ```sh
   msgfmt --statistics -c -o /dev/null po/<lang>/ktomato.po
   ```
   A finished catalog reports only translated messages.

The build picks up a new catalog automatically; to offer the language in the selectors, also add it to the list in `src/core/AppSettings.cpp` and to the language boxes in `src/qml/pages/AppSettingsPage.qml` and `src/qml/pages/WelcomePage.qml`. The desktop entry (`data/io.github.mineraleyt.ktomato.desktop`), the runner definition and `data/ktomato.notifyrc` carry their translations as `Key[lang]=` lines.

---

## Releasing

A release is a version bump, a commit, an annotated tag and a push. The release workflow builds the Flatpak bundle and publishes the GitHub release when it sees a `v*` tag.

**Checklist** for version `X.Y.Z`:

1. `CMakeLists.txt`: `project(kTomato VERSION X.Y.Z …)`.
2. `data/io.github.mineraleyt.ktomato.metainfo.xml`: add a new `<release version="X.Y.Z" date="YYYY-MM-DD">` **above** the previous ones, with a short list of what changed.
3. `po/*/ktomato.po`: `Project-Id-Version: kTomato X.Y.Z`.
4. Regenerate and merge the translations if strings changed (see [Translating](#translating)).
5. Build and run the whole test suite in a clean environment.
6. Validate the metadata:
   ```sh
   appstreamcli validate --no-net data/io.github.mineraleyt.ktomato.metainfo.xml
   desktop-file-validate data/io.github.mineraleyt.ktomato.desktop \
       data/io.github.mineraleyt.ktomato.runner.desktop
   ```
7. Commit (`Release vX.Y.Z: …`), then tag and push:
   ```sh
   git tag -a vX.Y.Z -m "kTomato X.Y.Z: …"
   git push origin main vX.Y.Z
   ```

The workflow refuses to publish unless the tag, the version in `CMakeLists.txt` and the newest `<release>` in the metainfo all agree. Pushes that only change Markdown files or `docs/` do not trigger a build or a release.
