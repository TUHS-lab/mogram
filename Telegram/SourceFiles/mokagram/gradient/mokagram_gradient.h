// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mokagram: change (fork): animated gradient chat background.
#pragma once

#include "base/basic_types.h"

class QWidget;
class QPainter;

namespace Mokagram::Gradient {

void RegisterSettings();

// widget перерисовывается, пока идёт анимация (последний нарисованный фон).
void Attach(not_null<QWidget*> widget);

// true: фон нарисован, штатные обои Telegram рисовать не нужно.
[[nodiscard]] bool Paint(QPainter &p, QSize fill, QRect clip, bool paused);

} // namespace Mokagram::Gradient
