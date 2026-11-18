// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 4c8170a2
// Mogram: экономия ресурсов. Фоновая экономия, режим тостера, возврат памяти системе.
#pragma once

#include <rpl/producer.h>

namespace Mogram::Perf {

// true: клиент должен вести себя в режиме экономии (окно в фоне или включён режим тостера).
[[nodiscard]] rpl::producer<bool> EconomyValue();

// Нужно ли сейчас реагировать на подвисания: когда окно в фоне, монитор лагов спит.
[[nodiscard]] bool UserIsLooking();

void Start();

} // namespace Mogram::Perf
