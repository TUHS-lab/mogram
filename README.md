<p align="center">
  <img src="docs/readme/logo.png" alt="Mogram" width="120">
</p>

<h1 align="center">Mogram</h1>

<p align="center">
  Telegram Desktop that looks and behaves the way you want.<br>
  One dark theme, rounded corners everywhere, music, plugins and privacy without compromise.
</p>

<p align="center">
  <a href="README.ru.md">Русский</a> · <b>English</b>
</p>

<p align="center">
  <a href="https://github.com/TUHS-lab/mogram/releases/latest"><img src="https://img.shields.io/badge/release-v0.2.0-0a84ff?style=for-the-badge&labelColor=282828" alt="Release v0.2.0"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPLv3-8e8e93?style=for-the-badge&labelColor=282828" alt="GPLv3"></a>
  <a href="https://t.me/tuhss"><img src="https://img.shields.io/badge/author-@tuhss-8e8e93?style=for-the-badge&labelColor=282828&logo=telegram&logoColor=white" alt="@tuhss"></a>
</p>

## About

**Mogram** is a Telegram Desktop fork for people who want more than the stock client. The interface is rebuilt from scratch: a single graphite theme, soft rounded corners on every window, cards instead of flat lists, adaptive panels. It stays fast and light on your system.

You sign in with a regular Telegram account on the regular servers. The client **does not collect data** and **does not depend on the author's servers**: no telemetry, no crash report uploads, no referral links. What is stored and where the client connects is described in [PRIVACY.md](PRIVACY.md).

## Features

- **Privacy**: streamer mode hides your number, username and ID; ghost mode does not mark messages as read and hides that you are online
- **Deleted messages and edits**: deleted messages stay with you with a marker, previous versions of edits are kept
- **Security**: decoy passcodes, lock on focus loss and with the computer, wipe after failed attempts
- **Music**: SoundCloud and chat music in one player, lyrics with line highlighting, recommendations based on your taste. The taste profile is computed only on your computer
- **Profile**: colored header with action buttons, a strip with the track that is playing, smooth collapsing
- **Customization**: accent colors, gradient background, element sizes. Every feature is toggled separately
- **Quick commands** and **tools**, for example a screenshot of messages as a nice card
- **System**: anti-lag mode, background resource saving
- **Plugins** in the `.plugin` format: a native C++ engine, a store inside the client, limits and an action log

Everything lives in one place: **Settings → Mogram Settings**. Details: [docs/FEATURES.md](docs/FEATURES.md) (Russian).

## Download

You do not need to build anything: download a ready build.

| System | Version | File |
|---|---|---|
| **Windows x64** | v0.2.0 | [mogram-win-x64.zip](https://github.com/TUHS-lab/mogram/releases/download/v0.2.0/mogram-win-x64.zip): unzip and run `Mogram.exe` |
| **macOS** | build once | one command: [docs/MACOS.md](docs/MACOS.md) |
| **Linux x86_64** | build once | one command, Docker does the rest: [docs/LINUX.md](docs/LINUX.md) |

The Windows build is not signed, so the system may warn you on first launch: choose "More info", then "Run anyway".

## Plugins

A plugin is a single `.plugin` file with a class that extends `BasePlugin`. Metadata, permissions and commands live in the file itself, the limit is 300 actions per minute and every action is logged. The native C++ engine runs the supported API constructs without an embedded Python interpreter; arbitrary Python code is not supported.

Full description: [docs-plugins-api.md](docs-plugins-api.md), example: [examples/showcase.plugin](examples/showcase.plugin).

## Building

Mogram is built the same way as Telegram Desktop:

- [macOS](docs/MACOS.md)
- [Windows](docs/building-win.md)
- [GNU/Linux](docs/LINUX.md)

Repository layout: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). The Telegram Desktop sources and the list of third-party libraries: [telegramdesktop/tdesktop](https://github.com/telegramdesktop/tdesktop).

## Credits and license

> **Based on Mogram, by [@tuhss](https://t.me/tuhss) (https://t.me/tuhss). https://github.com/TUHS-lab/mogram**

Mogram is based on [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) (© The Telegram Desktop Authors, GPLv3 with the OpenSSL exception). Modifications © 2026 [@tuhss](https://t.me/tuhss), same license, GPLv3, plus an attribution term.

Anyone using Mogram source code or builds **must credit the author** ([@tuhss](https://t.me/tuhss), https://github.com/TUHS-lab/mogram) in the README and the About window and keep the [notice](MOGRAM_NOTICE.md). If you do not, the license terminates.
