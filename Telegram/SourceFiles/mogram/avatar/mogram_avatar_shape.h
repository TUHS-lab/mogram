// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: f2f6d32e
// Mogram: формы аватарок (круг, сердце, звезда и т.д.) отдельно для людей,
// ботов, групп, каналов и форумов.
#pragma once

#include <QtGui/QImage>

class PeerData;

namespace Mogram::Avatar {

void RegisterSettings();

// 0: оставить как в Telegram. Иначе номер формы.
[[nodiscard]] int ShapeFor(not_null<const PeerData*> peer);

// Форма, выбранная для аватарки, которую сейчас строит кеш.
[[nodiscard]] int Current();

struct Scope {
	explicit Scope(int shape);
	~Scope();

private:
	int _old = 0;
};

// Вырезает форму из квадратной картинки (сглаженный край).
[[nodiscard]] QImage ApplyShape(const QImage &square, int shape);

} // namespace Mogram::Avatar
