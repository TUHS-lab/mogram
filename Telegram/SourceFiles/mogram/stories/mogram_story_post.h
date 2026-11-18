// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: публикация истории с компьютера (фото или видео).
#pragma once

#include "base/basic_types.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Mogram::Stories {

// Проверяет лимит, просит выбрать файл и открывает окно публикации.
void StartPosting(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

[[nodiscard]] bool CanPost(not_null<PeerData*> peer);

// Отладка: окно публикации с файлом, без выбора и без отправки.
void DebugShowCompose(
	not_null<Window::SessionController*> controller,
	const QString &path);

} // namespace Mogram::Stories
