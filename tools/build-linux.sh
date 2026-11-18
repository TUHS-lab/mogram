#!/usr/bin/env bash
# Mogram: build for Linux in one command (uses Docker).
# Сборка Mogram на Linux одной командой (через Docker).
#
# Usage / Запуск:  ./tools/build-linux.sh
# Result / Результат: mogram-linux-x64.tar.xz in the repository root.
set -euo pipefail

cd "$(dirname "$0")/.."
root="$PWD"

need() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "Missing / Не найдено: $1. $2" >&2
		exit 1
	fi
}
need git "Install git."
need docker "Install Docker and make sure your user can run it (docker ps)."
need poetry "Install poetry: https://python-poetry.org"

# Your own Telegram API credentials: https://core.telegram.org/api/obtaining_api_id
# Свои ключи Telegram API: https://core.telegram.org/api/obtaining_api_id
# They are asked here and passed only to the build; nothing is saved to disk.
# Они вводятся здесь и идут только в сборку, на диск не сохраняются.
api_id="${TDESKTOP_API_ID:-}"
api_hash="${TDESKTOP_API_HASH:-}"
if [ -z "$api_id" ]; then
	read -r -p "api_id: " api_id
fi
if [ -z "$api_hash" ]; then
	read -r -s -p "api_hash: " api_hash
	echo
fi
if [ -z "$api_id" ] || [ -z "$api_hash" ]; then
	echo "api_id and api_hash are required / нужны api_id и api_hash" >&2
	exit 1
fi

echo "== 1/3 Libraries image (the first run takes 1-3 hours, then it is cached) =="
echo "== 1/3 Образ с библиотеками (первый раз 1-3 часа, потом из кэша) =="
./Telegram/build/prepare/linux.sh

echo "== 2/3 Building Mogram / Сборка Mogram =="
docker run --rm \
	-u "$(id -u)" \
	-v "$root:/usr/src/tdesktop" \
	tdesktop:centos_env \
	/usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
	-D TDESKTOP_API_ID="$api_id" \
	-D TDESKTOP_API_HASH="$api_hash" \
	-D DESKTOP_APP_DISABLE_AUTOUPDATE=ON \
	-D DESKTOP_APP_DISABLE_CRASH_REPORTS=ON

echo "== 3/3 Packaging / Упаковка =="
if [ ! -f out/Release/Telegram ]; then
	echo "Build failed: out/Release/Telegram not found" >&2
	exit 1
fi
tar -C out/Release -cJf mogram-linux-x64.tar.xz Telegram

echo
echo "Done / Готово: $root/mogram-linux-x64.tar.xz"
echo "Run / Запуск:  ./out/Release/Telegram"
