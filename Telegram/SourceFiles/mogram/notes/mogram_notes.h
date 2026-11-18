// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: a49a39f0
// Mogram: личные теги и заметки о собеседниках (хранятся только у тебя).
#pragma once

#include "base/basic_types.h"
#include "data/data_types.h"

#include <rpl/producer.h>

namespace Mogram::Notes {

[[nodiscard]] QString Tag(PeerId id);
[[nodiscard]] QString Note(PeerId id);
void SetTag(PeerId id, const QString &tag);
void SetNote(PeerId id, const QString &note);

// Срабатывает при любом изменении тега или заметки. Значение: чей именно.
[[nodiscard]] rpl::producer<PeerId> Changed();
[[nodiscard]] rpl::producer<QString> NoteValue(PeerId id);

// Рисует плашку тега: возвращает занятую ширину (0, если не влезло).
int PaintTag(
	QPainter &p,
	const QString &tag,
	int left,
	int top,
	int maxRight,
	const QColor &color);

} // namespace Mogram::Notes
