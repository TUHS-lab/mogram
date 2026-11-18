// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: шифрование секретов на диске (токены входа и подобное).
#pragma once

#include <QtCore/QByteArray>

namespace Mogram::Secure {

// Шифрует данные ключом текущего пользователя ОС (Windows DPAPI). Файл
// нельзя расшифровать под другой учётной записью или на другом компьютере.
// Где защита ОС недоступна, данные возвращаются как есть.
[[nodiscard]] QByteArray Seal(const QByteArray &plain);

// Обратное действие. Старые файлы без шифрования читаются как раньше и при
// следующей записи сохраняются уже зашифрованными. Пустой результат значит,
// что расшифровать не удалось.
[[nodiscard]] QByteArray Open(const QByteArray &stored);

} // namespace Mogram::Secure
