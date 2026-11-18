// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 283e9107
// Mogram: английский вариант интерфейса. Русский текст остаётся ключом.
#pragma once

namespace Mogram::Lang {

// Русский и украинский/белорусский интерфейс Telegram: текст как есть.
// Остальные языки: английский перевод, а если его нет, русский.
[[nodiscard]] const char *Tr(const char *russian);

} // namespace Mogram::Lang
