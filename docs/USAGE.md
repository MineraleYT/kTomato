<!--
  SPDX-FileCopyrightText: 2026 kTomato contributors
  SPDX-License-Identifier: CC-BY-SA-4.0
-->

# kTomato user guide

This guide describes how to use kTomato day to day. For installation see the [README](../README.md); to build or contribute see the [development guide](DEVELOPMENT.md).

**Contents**

1. [Timers](#timers)
2. [The tray icon and its menu](#the-tray-icon-and-its-menu)
3. [Keyboard shortcuts](#keyboard-shortcuts)
4. [Statistics](#statistics)
5. [Task notes](#task-notes)
6. [Calendar events](#calendar-events)
7. [KRunner](#krunner)
8. [D-Bus and scripting](#d-bus-and-scripting)
9. [Data, backup and privacy](#data-backup-and-privacy)
10. [Updates and diagnostics](#updates-and-diagnostics)
11. [Flatpak notes](#flatpak-notes)

---

## Timers

A *timer* (preset) bundles the durations of a work phase, a short break and a long break, how many work sessions come before a long break, and a few options. kTomato ships with **Pomodoro** (25/5/15), **Deep Work**, **Study** and **Quick Sprint**; you can edit, duplicate and delete them or create your own in the **Timers** page.

- **Name, category and icon** appear in the timer list, the tray menu and the statistics. The category is also used as the label of the phase chip while you work.
- **Auto-start** decides what happens when a phase ends: *Manual* (nothing starts by itself), *Breaks only*, or *Everything*.
- **Silence notifications while working**, **play a sound** and **show a notification** when a phase ends can be set per timer.
- The timer you pick stays selected the next time you start kTomato. You cannot switch timers while one is running.

Pausing keeps the remaining time; stopping resets the phase. A phase stopped early is recorded as *interrupted* in the statistics if it ran for at least five seconds.

---

## The tray icon and its menu

The tray icon shows a ring with the minutes left (red while working, green during breaks). Its context menu contains, from the top:

1. **A status line** (not clickable): the phase, the time left and the name of the timer, for example `Work · 12:30 · Pomodoro`.
2. **Start / Pause / Resume**, and **Stop** and **Skip**, which appear only while a phase is running or paused.
3. **Timer**: a submenu with all your timers; pick one to switch to it. It is disabled while a timer is running.
4. **Statistics** and **Settings**, which open the window on that page.
5. **Restore / Minimize** and **Quit**, added by KDE.

Closing the window keeps kTomato running in the tray if *Settings → System tray → Keep running in the tray when the window is closed* is on. If your desktop has no system tray, the tray options are disabled and closing the window quits the app.

---

## Keyboard shortcuts

| Shortcut | Action |
|:---|:---|
| `Space` | Start or pause the timer (on the Timer page, when nothing else has the focus) |
| `Ctrl` + `1` … `5` | Go to Timer, Timers, Statistics, Settings, About |
| `Ctrl` + `,` | Open Settings |
| `Ctrl` + `S` / `Esc` | Save / cancel in the timer editor |
| `Ctrl` + `Q` | Quit |

For shortcuts that work from anywhere, bind the [D-Bus commands](#d-bus-and-scripting) in **System Settings → Shortcuts → Add Command**.

---

## Statistics

The **Statistics** page offers day, week, month and year views with:

- work and break time, completed and interrupted sessions;
- the daily goal (set in Settings, or turn it off by setting it to 0) and your streak; with *weekend streak protection* an inactive Saturday or Sunday does not break the streak;
- a chart of the period and a distribution by timer, category or task;
- **Export CSV** for the period you are looking at (UTF-8 with a byte-order mark, so spreadsheets show accents correctly).

Sessions are counted on the day they started, in your time zone. The page follows the clock: after midnight it moves on to the new day.

---

## Task notes

When a work session ends, kTomato can ask what you worked on (**Settings → Productivity and Goals → Prompt for a task note when a work session finishes**). Type it in the notification's reply field or in the in-app dialog, which also suggests recent notes. Notes are stored with the session, shown in the *By Task* statistics and included in the CSV export.

---

## Calendar events

kTomato can add an event to your calendar every time you **finish a work session**, for example "Pomodoro, 25 minutes". Breaks and interrupted phases are not sent. It is **off by default** and set up in **Settings → Calendar**.

### With Nextcloud

1. Choose **Nextcloud** as the calendar service and type your server address (for example `https://cloud.example.org`).
2. Press **Log in** and authorize kTomato in the browser tab that opens. kTomato receives an *app password* by itself; you do not copy anything.
3. If you have several calendars, pick one in the **Calendar** list. The refresh button next to it reloads the list from the server.
4. Press **Test** to check the connection: a green check on the button means it works.

In Nextcloud (*Settings → Security → Devices & sessions*) the new entry is named **kTomato**. Delete it there to revoke kTomato's access, and press **Disconnect** in kTomato to forget the login.

### With another CalDAV calendar

Choose **Custom CalDAV** and enter the calendar's CalDAV address, your user name and a password (use an *app password* if your provider offers them). Press the refresh button to list the calendars at that address and pick one. Nextcloud and Custom CalDAV keep **separate settings**, so switching between them does not mix up addresses or passwords.

### Good to know

- **Subscription links do not work.** A calendar link ending in `.ics` is read-only: it lets you *see* events, not add them. Use the calendar's CalDAV address.
- **What is sent:** the timer name, its category and the real start and end times. The task note goes in the event description only if you turn on *Include the task note in the event description*.
- **Secure connections only:** addresses must be `https://` (plain `http://` is accepted only for `localhost`). Certificate errors are never ignored.
- **If the server is unreachable**, events wait in a queue (up to 100) and kTomato retries every few minutes while it is running. Events still waiting when you quit are lost.
- **Where the password is kept:** in `~/.config/ktomatorc`, a file readable only by you. It is never written to logs or to the diagnostics report.
- **Not supported:** Proton Calendar (to our knowledge it offers no way for other apps to add events), and calendars that only accept an OAuth sign-in, such as Google Calendar or Microsoft 365.

---

## KRunner

kTomato adds a runner to the Plasma launcher. Press `Alt` + `Space` (or `Meta`) and type:

| Query | What it does |
|:---|:---|
| `pomodoro`, `ktomato`, `tomato` or `timer` | Shows the current status, the time left and quick actions |
| `pomodoro start` | Start or resume the timer |
| `pomodoro pause` / `pomodoro resume` | Pause or continue the countdown |
| `pomodoro stop` | Stop and reset |
| `pomodoro skip` | Go to the next phase |
| `pomodoro <timer name>` (or part of it) | Switch to that timer and start it |
| `pomodoro 25` (any number of minutes, 1–180) | Start a one-off work session of that length; your timer's own duration is not changed |

While a timer runs, the status result also offers buttons (select it, or press `Shift` + `Enter`): **Pause/Resume**, **Skip** and **Stop**.

Opening KRunner does **not** start kTomato; it starts (hidden in the tray) only when you type one of the words above. After installing or updating, restart KRunner once (`kquitapp6 krunner`) so that it reads the new plugin.

---

## D-Bus and scripting

kTomato exposes a D-Bus interface:

- **Service**: `io.github.mineraleyt.ktomato`
- **Path**: `/Timer`
- **Interface**: `io.github.mineraleyt.ktomato.Timer`

| Command | Action |
|:---|:---|
| `qdbus io.github.mineraleyt.ktomato /Timer toggle` | Start or pause |
| `qdbus io.github.mineraleyt.ktomato /Timer start` | Start or resume the active timer |
| `qdbus io.github.mineraleyt.ktomato /Timer pause` | Pause |
| `qdbus io.github.mineraleyt.ktomato /Timer stop` | Stop and reset |
| `qdbus io.github.mineraleyt.ktomato /Timer skip` | Skip to the next phase |
| `qdbus io.github.mineraleyt.ktomato /Timer status` | Print `state:phase:seconds` |

`status` returns three colon-separated fields, for example `running:work:1342`:

- **state**: `idle`, `running` or `paused`;
- **phase**: `work`, `short_break` or `long_break` (when idle, the phase that starts next);
- **seconds**: time left in the phase, in whole seconds.

Depending on your distribution, `qdbus` may be called `qdbus6` or `qdbus-qt6`. If kTomato is not running, calling the service starts it hidden in the system tray (D-Bus activation). You can also start it that way yourself with `ktomato --background`.

---

## Data, backup and privacy

| What | Where |
|:---|:---|
| Timers, sessions, notes | `~/.local/share/ktomato/ktomato.db` (SQLite) |
| Settings | `~/.config/ktomatorc` |

Inside the Flatpak the same files live under `~/.var/app/io.github.mineraleyt.ktomato/`.

**Settings → Data management** can **back up** the database to a file, **restore** a backup (including backups made by older versions), and **clear the session history**. Restoring replaces your current timers and sessions, and is not possible while a timer is running. If the database file is damaged, kTomato moves it aside (`ktomato.db.corrupt-…`) and starts a new one.

**What leaves your computer.** Nothing, unless you ask for it:

- **Check for updates** (About page) contacts the GitHub releases API and sends only the program version.
- **Calendar sync**, if you turn it on, sends the information described [above](#good-to-know) to the server you chose.

There is no telemetry and no automatic update check.

---

## Updates and diagnostics

- **About → Check for Updates** tells you whether a newer release exists and links to it. A successful check with nothing new shows a green check on the button for a few seconds.
- **Settings → Troubleshooting** can copy or export a report with the version, your environment and the latest log lines. Your home directory, user name, passwords and addresses with credentials are replaced with placeholders. Attach it to a bug report.

---

## Flatpak notes

The Flatpak has network access (for the update check and calendar sync), the notification and screen-saver services (to silence notifications and keep the screen awake), the system tray, and audio. *Start at login* uses the desktop's background portal, which may ask for your permission the first time.
