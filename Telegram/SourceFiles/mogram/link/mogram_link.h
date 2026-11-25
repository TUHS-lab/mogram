// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: Mogram Link, связь ПК и телефона через закрытый канал в вашем аккаунте.
// Протокол MGL1 описан в docs/mogram-link.md.
#pragma once

#include "base/basic_types.h"

namespace Mogram::Link {

void RegisterSettings();
void Start();

// Читают ли чат на другом устройстве прямо сейчас (последние 25 секунд).
[[nodiscard]] bool ActiveElsewhere(quint64 peerValue);

} // namespace Mogram::Link
