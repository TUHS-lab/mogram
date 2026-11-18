# Устройство репозитория

```text
Telegram/SourceFiles/mogram/
├── plugins/    движок плагинов, магазин, права
├── msginfo/    окно «Информация о сообщении»
├── music/      плеер
├── streamer/   режим стримера
├── ghost/      призрак
├── commands/   быстрые команды
└── settings/   настройки Mogram
patches/        правки поверх upstream
```

Всё добавленное лежит в `Telegram/SourceFiles/mogram/`, остальные правки помечены `Mogram:`. Так форк проще подтягивать к свежему Telegram Desktop.
