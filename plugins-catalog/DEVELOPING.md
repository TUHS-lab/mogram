# Разработка плагинов Mogram

Как запустить свой плагин в отладке и какие ручки есть у API помимо сообщений. Оверлеи, геометрия интерфейса, события интерфейса и режим отладки есть начиная с Mogram 0.1.3.

Подсказки в редакторе: `mogram_sdk/base_plugin.py` в этом репозитории совпадает с клиентским (хуки, `Switch`, `Input`, `MenuItem`, `command`, `watcher`). Положи папку `mogram_sdk` рядом с плагином или в `PYTHONPATH` редактора.

## Режим отладки

Отдельный экземпляр клиента, свои данные, твоя папка плагинов, живая сессия не трогается:

```bash
mkdir -p ~/plug && cp my.plugin ~/plug/
MOGRAM_PLUGIN_DEBUG=1 MOGRAM_PLUGINS_DIR=~/plug ./Telegram -workdir ~/mog-dev
```

Что включается с `MOGRAM_PLUGIN_DEBUG=1`:

- **Автоперезагрузка.** Сохранил `*.plugin` в папке, через полсекунды плагин выгружен и загружен заново. Новые файлы подхватываются. Подтверждать изменённый файл не нужно, хеш обновляется сам.
- **Журнал каждого вызова.** В лог клиента (`<workdir>/log.txt`) строки `[dev] <id>: <операция> <цель> -> allowed|denied`. Видно, что плагин вызвал и что ему разрешили или отказали.
- **Полные трейсбеки** ошибок в хуках, командах и колбэках.
- **Плагин не отключается.** Автоотключение за 20 ошибок в минуту, выключение хука после 5 ошибок подряд и после 3 просрочек в debug не срабатывают, всё только пишется в лог.
- **Лимиты частоты выключены** (300 действий в минуту и остальные). Права из `__permissions__` выдаются как обычно, опасный уровень L4 не выдаётся.

### Команды

Пишешь строку в файл `~/plug/.dev/cmd`, клиент читает его каждые полсекунды, очищает и пишет ответ в лог и в `~/plug/.dev/out.txt`:

```bash
echo "list" >> ~/plug/.dev/cmd                       # все плагины и состояние
echo "reload my_plugin" >> ~/plug/.dev/cmd           # перезагрузить вручную
echo "state my_plugin" >> ~/plug/.dev/cmd            # состояние, выключенные хуки, таймеры, оверлеи
echo "calls my_plugin 50" >> ~/plug/.dev/cmd         # последние 50 вызовов API
echo "eval my_plugin self.x" >> ~/plug/.dev/cmd      # выражение в пространстве имён плагина
echo "eval my_plugin print(api.theme.get())" >> ~/plug/.dev/cmd
```

В `eval` доступны всё из модуля плагина и `self` (экземпляр плагина). Выражение печатается как `= значение`, оператор выполняется без вывода.

### Проверка файла без запуска

`python3 lint.py my.plugin` проверяет метаданные и опасные места, не исполняя код плагина.

## Оверлеи: питомцы и всё, что рисуется поверх окна

Право `ui` (есть у всех плагинов). Координаты: пиксели области окна клиента, начало в левом верхнем углу.

```python
import mogram_sdk.api as api

ov = api.overlay.create("pet", w=160, h=120, click_through=False)   # до 4 слоёв на плагин
ov.set_sprite(["..rr..", ".rrrr."], {"r": "#ff5050"}, scale=8)        # сетка + палитра, пиксели без сглаживания
ov.move(200, 300)                                                   # левый верхний угол слоя
ov.say("привет", 3000)                                              # пузырь над слоем, мс
ov.on_tick(lambda dt: ..., fps=30)                                  # 5..60 кадров в секунду, dt в мс
ov.on_pointer(lambda e: ...)                                        # down/move/up/double, x, y, vx, vy, button
ov.destroy()
```

- `create` можно звать в `on_plugin_load`: пока окно не готово, вызовы копятся и применяются сами (`ov.ready`).
- `click_through=True`: клики проходят сквозь слой. `False`: слой ловит указатель только в своём прямоугольнике, остальное окно работает как обычно.
- Спрайт рисуется нижней кромкой слоя по центру. Цвета: `'#rrggbb'`, `'#aarrggbb'`, `(r, g, b[, a])`, `None` для прозрачного.
- `api.window.bounds()` даёт `{w, h}` области окна, `api.theme.get()` даёт `{dark, window_bg, window_fg, window_sub_fg, accent}`.
- Кадры (`move`, `set_sprite`, `on_tick`) не входят в общий лимит 300 действий в минуту, у них свой: 120 в секунду.

## Геометрия интерфейса

Право `ui`. Все прямоугольники `{x, y, w, h}` в тех же координатах, что у оверлеев.

| Вызов | Что возвращает |
|---|---|
| `api.ui.message_rects()` | видимые сообщения `[{id, peer, out, x, y, w, h}]`, строка по ширине ленты |
| `api.ui.composer_rect()` | поле ввода или `None`, если чат не открыт |
| `api.ui.header_rect()` | шапка открытого чата или `None` |
| `api.ui.history_rect()` | вся лента сообщений или `None` |
| `api.ui.elements()` | видимые кнопки `[{id, kind, hidden, x, y, w, h}]`, `id` живёт до следующего вызова |
| `api.ui.hide(id)` / `api.ui.show(id)` | спрятать кнопку (невидима и не нажимается) и вернуть обратно |

Вызовы ограничены 30 в секунду, `elements()` 10 в секунду.

## События интерфейса

Подписка автоматическая: достаточно переопределить метод. Право `ui`, `messages.read` не нужно.

| Метод | Payload |
|---|---|
| `on_scroll(info)` | `{dy, top}` прокрутка ленты в пикселях |
| `on_theme_changed(theme)` | как `api.theme.get()` |
| `on_pointer_move(pos)` | `{x, y}` курсор в координатах окна, до 30 раз в секунду |
| `on_window_shake(info)` | `{source: "cursor" \| "window"}` резкая тряска курсора или окна |
| `on_media_state(state)` | `{playing, title, kind: "music" \| "voice", position_ms, length_ms}` |

Пока ни один плагин не подписан на `scroll`, `pointer_move` и `window_shake`, опрос не работает.

## Старые версии клиента и Windows

- Поле `__min_version__` только информация для магазина, клиент по нему плагин не отключает. Если плагин должен жить и на старых версиях, проверяй наличие ручки: `if hasattr(api, "overlay"): ...`.
- На Windows `python312.dll` лежит рядом с `Mogram.exe`. Если её нет (архив не распакован целиком), клиент стартует, но плагины выключены, а в журнале строка «нет библиотеки Python рядом с клиентом». Для разработки собирай и запускай из полностью распакованного архива.
- `on_theme_changed` приходит только когда цвета действительно изменились, а не при каждой перерисовке палитры.

## Справка: то, о чём часто спрашивают

- **`on_chat_opened(peer)`**: `dict` с ключами `peer` (id строкой), `title`, `username`, `kind` (`user`, `bot`, `group`, `channel`). Нужно право `messages.read`.
- **Команды**: обработчик получает `ctx`. `ctx.text` весь текст сообщения, `ctx.args` аргументы одной строкой, `ctx.args_list` аргументы списком (с учётом кавычек), `ctx.args_entities` сущности аргументов.
- **Настройки** (`create_settings`), поля: `Header(text)`, `Divider(text=None)`, `Text(text, subtext, icon, on_click)`, `Button(text, subtext, icon, on_click)`, `Switch(key, text, default, subtext, icon, on_change)`, `Selector(key, text, items, default, icon, on_change)`, `Input(key, text, default, subtext, icon, on_change)`, `Password(key, text, subtext, icon)`.
- **Из экземпляра плагина** доступны `self.get_setting(key, default)`, `self.set_setting(key, value)`, `self.show_toast(text, icon, duration)`, `self.get_secret(key)`, `self.set_secret(key, value)`, `self.logger`, `self.s(key)` для строк.
- **Параметры хуков** это обычные `dict`: `send_message_hook(params)`, `message_filter_hook(message)`, `on_new_message(message)`. Измени `params` и верни `HookResult(HookStrategy.MODIFY, params)`.
