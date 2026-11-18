<p align="center">
  <img src="docs/readme/logo.png" alt="Mogram" width="120">
</p>

<h1 align="center">Mogram</h1>

<p align="center">
  Telegram Desktop, который выглядит и ведёт себя так, как хочешь ты.<br>
  Единая тёмная тема, скругления везде, музыка, плагины и приватность без компромиссов.
</p>

<p align="center">
  <b>Русский</b> · <a href="README.md">English</a>
</p>

<p align="center">
  <a href="https://github.com/TUHS-lab/mogram/releases/latest"><img src="https://img.shields.io/badge/%D1%80%D0%B5%D0%BB%D0%B8%D0%B7-v0.2.0-0a84ff?style=for-the-badge&labelColor=282828" alt="Релиз v0.2.0"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/%D0%BB%D0%B8%D1%86%D0%B5%D0%BD%D0%B7%D0%B8%D1%8F-GPLv3-8e8e93?style=for-the-badge&labelColor=282828" alt="GPLv3"></a>
  <a href="https://t.me/tuhss"><img src="https://img.shields.io/badge/%D0%B0%D0%B2%D1%82%D0%BE%D1%80-@tuhss-8e8e93?style=for-the-badge&labelColor=282828&logo=telegram&logoColor=white" alt="@tuhss"></a>
</p>

## О проекте

**Mogram** - форк Telegram Desktop для тех, кому мало стандартного клиента. Интерфейс собран заново: единая графитовая тема, мягкие скругления у каждого окна, карточки вместо плоских списков, адаптивные панели. Всё работает быстро и не грузит систему.

Вход в обычный аккаунт Telegram, обычные серверы. Клиент **не собирает данные** и **не зависит от серверов автора**: ни телеметрии, ни отправки отчётов о сбоях, ни рекламных ссылок. Что хранится и куда ходит клиент, описано в [PRIVACY.md](PRIVACY.md).

## Что умеет

- **Приватность**: режим стримера прячет номер, юзернейм и ID; режим призрака не отмечает сообщения прочитанными и скрывает, что ты в сети
- **Удалённые и правки**: удалённые сообщения остаются у тебя с пометкой, прошлые версии правок сохраняются
- **Безопасность**: ложные пароли, блокировка при потере фокуса и вместе с компьютером, стирание после неверных попыток
- **Музыка**: SoundCloud и музыка из чатов в одном плеере, текст песни с подсветкой строки, подборка по твоему вкусу. Профиль вкуса считается только на твоём компьютере
- **Профиль**: цветная шапка с кнопками действий, плашка с играющей музыкой, плавное сворачивание
- **Кастомизация**: акцентные цвета, градиентный фон, размеры элементов. Каждая функция включается отдельно
- **Быстрые команды** и **инструменты**: например, скриншот сообщений красивой карточкой
- **Система**: анти-лаг, экономия ресурсов в фоне
- **Плагины** в формате `.plugin`: нативный движок на C++, магазин прямо в клиенте, лимиты и журнал действий

Всё лежит в одном месте: **Настройки → Настройки Mogram**. Подробно: [docs/FEATURES.md](docs/FEATURES.md).

## Скачать

| Система | Версия | Файл |
|---|---|---|
| **macOS Apple Silicon (arm64)** | v0.1.8 | [mogram-arm64.dmg](https://github.com/TUHS-lab/mogram/releases/download/v0.1.8/Mogram-0.1.8-arm64.dmg) |
| **Windows x64** | v0.2.0 | [mogram-win-x64.zip](https://github.com/TUHS-lab/mogram/releases/download/v0.2.0/mogram-win-x64.zip) |
| **Linux x86_64** | собрать один раз | одна команда, остальное делает Docker: [docs/LINUX.md](docs/LINUX.md) |

Сборка для Windows без подписи разработчика, система может предупредить при первом запуске.

## Плагины

Плагин: один файл `.plugin` с классом, наследующим `BasePlugin`. Метаданные, права и команды лежат в самом файле, лимит 300 действий в минуту, каждое действие пишется в журнал. Нативный движок на C++ выполняет поддерживаемые конструкции API без встроенного интерпретатора Python; произвольный Python-код не поддерживается.

Полное описание: [docs-plugins-api.md](docs-plugins-api.md), пример: [examples/showcase.plugin](examples/showcase.plugin).

## Сборка

Mogram собирается так же, как Telegram Desktop:

- [macOS](docs/building-mac.md)
- [Windows](docs/building-win.md)
- [GNU/Linux (Docker)](docs/building-linux.md)

Устройство репозитория: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Исходники Telegram Desktop и список сторонних библиотек: [telegramdesktop/tdesktop](https://github.com/telegramdesktop/tdesktop).

## Авторство и лицензия

> **Основано на Mogram, автор [@tuhss](https://t.me/tuhss) (https://t.me/tuhss). https://github.com/TUHS-lab/mogram**

Mogram основан на [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) (© The Telegram Desktop Authors, GPLv3 с исключением OpenSSL). Доработки © 2026 [@tuhss](https://t.me/tuhss), лицензия та же, GPLv3, плюс условие об сохранении авторства.

Кто берёт исходники или сборки Mogram, обязан сохранить [уведомление](MOGRAM_NOTICE.md) и указать авторство в README и в окне «О программе». При нарушении лицензия прекращается.

*Anyone using Mogram source code **must credit the author** ([@tuhss](https://t.me/tuhss), https://github.com/TUHS-lab/mogram) in the README and the About window and keep the [notice](MOGRAM_NOTICE.md).*
