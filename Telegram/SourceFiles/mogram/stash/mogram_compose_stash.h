// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: отложенный черновик (Ctrl+S) переживает перезапуск клиента.
// Хранится текст, ответ и пересылка; файлы только до закрытия клиента.
#pragma once

#include "data/data_drafts.h"

class History;

namespace Data {
struct ComposeStash;
} // namespace Data

namespace Mogram::Stash {

void Persist(
	not_null<History*> history,
	Data::DraftKey key,
	const Data::ComposeStash *stash);

// Один раз на чат поднимает черновик с диска в History.
void RestoreIfNeeded(not_null<History*> history, Data::DraftKey key);

} // namespace Mogram::Stash
