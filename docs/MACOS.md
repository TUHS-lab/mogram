# Mogram on macOS / Mogram на macOS

[English](#english) · [Русский](#русский)

## English

There is no ready-made macOS build in the releases yet, so you build it once on your own Mac. It is one command: the script installs the build tools through Homebrew, builds the libraries and the app.

This guide was written from the project's build setup and has not been tested on every Mac. If a step fails, open an issue with the last lines of the output.

### What you need

- A Mac with a recent macOS (12 or newer is recommended), Apple Silicon or Intel
- [Xcode](https://apps.apple.com/app/xcode/id497799835) from the App Store, launched once so it finishes installing
- [Homebrew](https://brew.sh)
- about 55 GB of free disk space
- your own Telegram API credentials (`api_id` and `api_hash`), get them at <https://core.telegram.org/api/obtaining_api_id>

Once, point the tools to Xcode:

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

### Build

```bash
git clone https://github.com/TUHS-lab/mogram.git
cd mogram
./tools/build-mac.sh
```

The script asks for your `api_id` and `api_hash` (they are used only for this build and are not saved), installs the tools, builds the libraries and the app and packs it into `mogram-macos.zip`.

The first build takes 1 to 3 hours because the libraries are built once. Later builds reuse the cache and are much faster.

### Run

```bash
open out/Release/Mogram.app
```

The app is not signed, so macOS may refuse to open it from the zip. Right-click `Mogram.app`, choose **Open**, then confirm. If it still says the app is damaged, run once:

```bash
xattr -cr /path/to/Mogram.app
```

### Problems

- `xcode-select: error` or "Xcode not found": open Xcode once and run the `xcode-select` command above.
- The build stops with "no space left": free up disk space and run the script again, it continues from the cache.

## Русский

Готовой сборки для macOS в релизах пока нет, поэтому её собирают один раз на своём Mac. Это одна команда: скрипт сам поставит инструменты через Homebrew, соберёт библиотеки и приложение.

Гайд составлен по тому, как сборка устроена в проекте, и проверен не на всех Mac. Если шаг упал, создайте issue и приложите последние строки вывода.

### Что нужно

- Mac с современной macOS (рекомендуется 12 и новее), Apple Silicon или Intel
- [Xcode](https://apps.apple.com/app/xcode/id497799835) из App Store, один раз запущенный, чтобы он дошёл установку
- [Homebrew](https://brew.sh)
- около 55 ГБ свободного места
- свои ключи Telegram API (`api_id` и `api_hash`): <https://core.telegram.org/api/obtaining_api_id>

Один раз укажите системе, где Xcode:

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
```

### Сборка

```bash
git clone https://github.com/TUHS-lab/mogram.git
cd mogram
./tools/build-mac.sh
```

Скрипт спросит `api_id` и `api_hash` (они нужны только для этой сборки и никуда не сохраняются), поставит инструменты, соберёт библиотеки и приложение и упакует его в `mogram-macos.zip`.

Первая сборка занимает от 1 до 3 часов: библиотеки собираются один раз. Следующие сборки используют кэш и идут гораздо быстрее.

### Запуск

```bash
open out/Release/Mogram.app
```

Приложение не подписано, поэтому macOS может отказаться открывать его из архива. Нажмите на `Mogram.app` правой кнопкой, выберите **Открыть** и подтвердите. Если всё равно пишет, что приложение повреждено, выполните один раз:

```bash
xattr -cr /путь/к/Mogram.app
```

### Если что-то не работает

- `xcode-select: error` или «Xcode не найден»: один раз откройте Xcode и выполните команду `xcode-select` выше.
- Сборка остановилась с «no space left»: освободите место и запустите скрипт снова, он продолжит с кэша.
