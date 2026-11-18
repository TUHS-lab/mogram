// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 00b899a4
// Mogram: скачивание медиа по ссылке из сообщения через API Saveinator.
#pragma once

#include "base/basic_types.h"

class HistoryItem;

namespace Api {
struct SendAction;
} // namespace Api

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Mogram::Saveinator {

void RegisterSettings();
void Start();

[[nodiscard]] QString FindLink(not_null<HistoryItem*> item);

void AddMessageAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

[[nodiscard]] bool OfferSendDownload(
	not_null<Window::SessionController*> controller,
	const Api::SendAction &action,
	const QString &text,
	Fn<void()> sendAsText,
	Fn<void()> clearField);

} // namespace Mogram::Saveinator
