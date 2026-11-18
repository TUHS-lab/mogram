// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 20023ecb
// Mogram: картинка-цитата из сообщений, с анонимизацией имён и аватарок.
#pragma once

#include "data/data_msg_id.h"

#include <QtGui/QImage>

namespace Window {
class SessionController;
} // namespace Window

namespace Mogram::Quote {

void RegisterSettings();

void ShowQuoteBox(
	not_null<Window::SessionController*> controller,
	std::vector<FullMsgId> ids);

} // namespace Mogram::Quote
