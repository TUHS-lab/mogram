#!/usr/bin/env bash
# Mogram: build for macOS in one command.
# Сборка Mogram на macOS одной командой.
#
# Usage / Запуск:  ./tools/build-mac.sh
# Result / Результат: mogram-macos.zip with Mogram.app in the repository root.
set -euo pipefail

cd "$(dirname "$0")/.."
root="$PWD"

if [ "$(uname -s)" != "Darwin" ]; then
	echo "This script is for macOS only / Скрипт только для macOS" >&2
	exit 1
fi

need() {
	if ! command -v "$1" >/dev/null 2>&1; then
		echo "Missing / Не найдено: $1. $2" >&2
		exit 1
	fi
}
need git "Install Xcode command line tools: xcode-select --install"
need brew "Install Homebrew: https://brew.sh"
need xcodebuild "Install Xcode from the App Store, then run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer"

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

echo "== 1/4 Tools / Инструменты (Homebrew) =="
brew install automake libtool cmake wget pkg-config gnu-tar ninja nasm meson

echo "== 2/4 Libraries (the first run takes 1-3 hours, then it is cached) =="
echo "== 2/4 Библиотеки (первый раз 1-3 часа, потом из кэша) =="
./Telegram/build/prepare/mac.sh

echo "== 3/4 Building Mogram / Сборка Mogram =="
(
	cd Telegram
	./configure.sh \
		-D TDESKTOP_API_ID="$api_id" \
		-D TDESKTOP_API_HASH="$api_hash" \
		-D DESKTOP_APP_DISABLE_AUTOUPDATE=ON \
		-D DESKTOP_APP_DISABLE_CRASH_REPORTS=ON
)
cmake --build out --config Release

echo "== 4/4 Packaging / Упаковка =="
app="$(find out -maxdepth 3 -name 'Mogram.app' -type d | head -n 1)"
if [ -z "$app" ]; then
	echo "Build failed: Mogram.app not found" >&2
	exit 1
fi
rm -f mogram-macos.zip
ditto -c -k --keepParent "$app" mogram-macos.zip

echo
echo "Done / Готово: $root/mogram-macos.zip  (app: $root/$app)"
echo "Open / Запуск: open \"$root/$app\""
