// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 557f0a88
// Mogram: сниппеты. Пишешь ;имя, и при отправке оно заменяется сохранённым текстом.
#pragma once

#include "ui/text/text_entity.h"

namespace Mogram::Snippets {

[[nodiscard]] QStringList Names();
[[nodiscard]] TextWithTags Get(const QString &name);
void Set(const QString &name, const TextWithTags &text);

// Часть текста с разметкой (эмодзи, жирный и т.д.), сдвинутая к нулю.
[[nodiscard]] TextWithTags Slice(
	const TextWithTags &text,
	int from,
	int length);
bool Remove(const QString &name);

// Заменяет ;имя в тексте перед отправкой с сохранением разметки и премиум-эмодзи.
void Expand(TextWithTags &text);

// Шрифт сообщений из настроек: жирный, курсив и т. д. или буквы другого начертания.
// Не трогает команды плагинов (.команда, /команда) и текст, где уже есть своё форматирование.
void ApplyFont(TextWithTags &text);

} // namespace Mogram::Snippets
