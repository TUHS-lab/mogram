// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 81cc2956
// Mogram: эффекты удаления сообщений и частицы при наборе текста.
#pragma once

#include "base/basic_types.h"

class QWidget;
class QPainter;

namespace Ui {
class InputField;
} // namespace Ui

namespace Mogram::Effects {

void RegisterSettings();

// Свои анимированные обои чата (гиф или видео из файла). true: кадр нарисован.
// widget нужен, чтобы перерисовывать его, пока идёт анимация.
void WallpaperAttach(not_null<QWidget*> widget);
[[nodiscard]] bool PaintWallpaper(QPainter &p, QSize fill, QRect clip, bool paused);

// Короткая заставка при первом показе окна: знак проявляется и растворяется.
void PlayStartup(not_null<QWidget*> host);

// Сообщение в rectInSource (координаты source) рассыпается поверх host.
void PlayDelete(
	not_null<QWidget*> host,
	not_null<QWidget*> source,
	QRect rectInSource,
	bool outgoing);

} // namespace Mogram::Effects
