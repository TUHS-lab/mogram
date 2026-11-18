// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: change (fork): animated gradient chat background.
#pragma once

#include "base/basic_types.h"

class QWidget;
class QPainter;

namespace Mogram::Gradient {

void RegisterSettings();

// widget перерисовывается, пока идёт анимация (последний нарисованный фон).
void Attach(not_null<QWidget*> widget);

// Ещё один widget, который показывает кусок фона (например, угол рядом с
// списком чатов) и должен перерисовываться вместе с основным.
void AttachExtra(not_null<QWidget*> widget);

// true: фон нарисован, штатные обои Telegram рисовать не нужно.
[[nodiscard]] bool Paint(QPainter &p, QSize fill, QRect clip, bool paused);

} // namespace Mogram::Gradient
