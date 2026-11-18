/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_start.h"

#include "lang/lang_keys.h"
#include "lang/lang_instance.h"
#include "mogram/lang/mogram_lang.h"
#include "intro/intro_qr.h"
#include "intro/intro_phone.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "main/main_account.h"
#include "main/main_app_config.h"

namespace Intro {
namespace details {

StartWidget::StartWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data, true) {
	setMouseTracking(true);
	setTitleText(rpl::single(u"Mogram Desktop"_q));
	// Mogram: это неофициальный клиент, поэтому без слова «официальный».
	// Языковой пакет подгружается уже после показа этого шага: текст
	// пересчитывается при каждой смене языка, а не один раз при создании.
	setDescriptionText(rpl::single(
		rpl::empty
	) | rpl::then(
		Lang::GetInstance().updated()
	) | rpl::map([] {
		return QString::fromUtf8(Mogram::Lang::Tr(
			"Добро пожаловать в Mogram.\nБыстрый и безопасный клиент на основе Telegram Desktop."));
	}));
	show();
}

void StartWidget::submit() {
	account().destroyStaleAuthorizationKeys();
	goNext<QrWidget>();
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return tr::lng_start_msgs();
}

rpl::producer<> StartWidget::nextButtonFocusRequests() const {
	return _nextButtonFocusRequests.events();
}

void StartWidget::activate() {
	Step::activate();
	setInnerFocus();
}

void StartWidget::setInnerFocus() {
	_nextButtonFocusRequests.fire({});
}

} // namespace details
} // namespace Intro
