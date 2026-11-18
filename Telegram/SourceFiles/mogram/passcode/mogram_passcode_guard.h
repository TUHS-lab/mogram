// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: охрана код-пароля: стирание после неверных попыток,
// автоблокировка при уходе из окна и блокировке экрана компьютера.
#pragma once

#include "base/basic_types.h"

namespace Mogram::PasscodeGuard {

// Неверные попытки переживают перезапуск клиента.
void EnsureLoaded();

// true, если эта ошибка запустила стирание.
bool RegisterFailure();
void RegisterSuccess();

// Сколько ждать до следующей попытки.
[[nodiscard]] crl::time WaitLeft();
// -1, если стирание после попыток выключено.
[[nodiscard]] int AttemptsBeforeWipe();

// Автоблокировка: следит за окном и экраном компьютера.
void Init();
void ScreenLocked();

void RegisterSettings();

} // namespace Mogram::PasscodeGuard
