# Mogram on Linux / Mogram на Linux

[English](#english) · [Русский](#русский)

## English

There is no ready-made Linux build in the releases yet, so you build it once on your own machine. It is one command and Docker does the rest, nothing else gets installed on your system.

### What you need

- Linux x86_64 (any recent distribution)
- [Docker](https://docs.docker.com/engine/install/) and your user able to run it (`docker ps` works without `sudo`)
- `git` and [poetry](https://python-poetry.org)
- about 50 GB of free disk space
- your own Telegram API credentials (`api_id` and `api_hash`), get them at <https://core.telegram.org/api/obtaining_api_id>

### Build

```bash
git clone https://github.com/TUHS-lab/mogram.git
cd mogram
./tools/build-linux.sh
```

The script asks for your `api_id` and `api_hash` (they are used only for this build and are not saved), prepares the library image, builds the client and packs it into `mogram-linux-x64.tar.xz`.

The first build takes 1 to 3 hours because the libraries (Qt, ffmpeg and others) are built once. Later builds reuse the cache and are much faster.

### Run

```bash
./out/Release/Telegram
```

Or unpack the archive anywhere and run `./Telegram` from it. To keep your data in a folder of your choice, start it with `-workdir /path/to/folder`.

### Problems

- `permission denied` on Docker: add yourself to the `docker` group and log in again.
- The build stops with "no space left": free up disk space and run the script again, it continues from the cache.

## Русский

Готовой сборки для Linux в релизах пока нет, поэтому её собирают один раз у себя. Это одна команда, остальное делает Docker, в систему ничего лишнего не ставится.

### Что нужно

- Linux x86_64 (любой свежий дистрибутив)
- [Docker](https://docs.docker.com/engine/install/), и чтобы ваш пользователь мог его запускать (`docker ps` работает без `sudo`)
- `git` и [poetry](https://python-poetry.org)
- около 50 ГБ свободного места
- свои ключи Telegram API (`api_id` и `api_hash`): <https://core.telegram.org/api/obtaining_api_id>

### Сборка

```bash
git clone https://github.com/TUHS-lab/mogram.git
cd mogram
./tools/build-linux.sh
```

Скрипт спросит `api_id` и `api_hash` (они нужны только для этой сборки и никуда не сохраняются), подготовит образ с библиотеками, соберёт клиент и упакует его в `mogram-linux-x64.tar.xz`.

Первая сборка занимает от 1 до 3 часов: библиотеки (Qt, ffmpeg и другие) собираются один раз. Следующие сборки используют кэш и идут гораздо быстрее.

### Запуск

```bash
./out/Release/Telegram
```

Или распакуйте архив куда угодно и запустите `./Telegram`. Чтобы хранить данные в выбранной папке, запускайте с `-workdir /путь/к/папке`.

### Если что-то не работает

- `permission denied` у Docker: добавьте себя в группу `docker` и перезайдите в систему.
- Сборка остановилась с «no space left»: освободите место и запустите скрипт снова, он продолжит с кэша.
