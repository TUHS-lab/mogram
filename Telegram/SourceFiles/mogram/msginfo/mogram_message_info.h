// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: f1d391dd
// Mogram: окно «Информация о сообщении» (ID, даты, пересылка, вложение, кодировщик голосового).
#pragma once

class HistoryItem;

namespace Window {
class SessionController;
} // namespace Window

namespace Mogram::MessageInfo {

void Show(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

} // namespace Mogram::MessageInfo
