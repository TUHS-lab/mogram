// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: fbe4699b
// Mogram: оформление, настраиваемое в «Кастомизации» (пузыри сообщений, цвета плеера).
#pragma once

#include <QtGui/QColor>

namespace Mogram::Appearance {

void RegisterSettings();

// Применяет настройки к палитре и следит за их изменением и сменой темы.
void Start();

// Цвет из настройки вида "#rrggbb"; пустой или неверный даёт невалидный QColor.
[[nodiscard]] QColor ColorSetting(const QString &key);

// Сколько сообщений можно выделить за раз (100 или 3000 по настройке).
[[nodiscard]] int SelectionLimit();

} // namespace Mogram::Appearance
