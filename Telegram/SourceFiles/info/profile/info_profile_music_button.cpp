/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_music_button.h"

#include "ui/color_contrast.h"
#include "ui/effects/animation_value.h"
#include "ui/text/text_utilities.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/ui_utility.h"
#include "styles/style_chat.h"
#include "styles/style_info.h"

namespace Info::Profile {
namespace {

int MusicCount = 0;
std::vector<not_null<QWidget*>> MusicWidgets;
std::vector<not_null<QWidget*>> StickyWidgets;
rpl::event_stream<> MusicChanges;

} // namespace

namespace {
std::optional<QColor> MusicBand;
} // namespace

std::optional<QColor> MusicBandColor() {
	return MusicBand;
}

namespace {
float64 MusicFadeValue = 1.;
rpl::event_stream<> MusicFadeEvents;
} // namespace

void SetMusicFade(float64 value) {
	value = std::clamp(value, 0., 1.);
	if (std::abs(MusicFadeValue - value) < 0.004) {
		return;
	}
	MusicFadeValue = value;
	MusicFadeEvents.fire({});
}

float64 MusicFade() {
	return MusicFadeValue;
}

namespace {
bool MusicDetachedValue = false;
} // namespace

void SetMusicDetached(bool value) {
	if (MusicDetachedValue == value) {
		return;
	}
	MusicDetachedValue = value;
	MusicFadeEvents.fire({});
}

bool MusicDetached() {
	return MusicDetachedValue;
}

rpl::producer<> MusicFadeChanges() {
	return MusicFadeEvents.events();
}

bool MusicShown() {
	return MusicCount > 0;
}

MusicButton *FindMusicButton(not_null<QWidget*> inside) {
	for (const auto widget : MusicWidgets) {
		if (inside->isAncestorOf(widget) && widget->isVisible()) {
			return static_cast<MusicButton*>(widget.get());
		}
	}
	return nullptr;
}

bool MusicAttachedTo(not_null<QWidget*> bar) {
	const auto bottom = bar->mapToGlobal(QPoint(0, bar->height())).y();
	for (const auto widget : StickyWidgets) {
		if (!widget->isVisible()) {
			continue;
		}
		const auto gap = widget->mapToGlobal(QPoint()).y() - bottom;
		if (gap <= 40 && gap >= -style::ConvertScale(2)) {
			return true;
		}
	}
	for (const auto widget : MusicWidgets) {
		// Зазор между шапкой и плашкой бывает больше пикселя (масштаб, отступы).
		// Плашка, уехавшая вверх больше чем на 12 пикселей, уже не прилегает к шапке.
		const auto gap = widget->mapToGlobal(QPoint()).y() - bottom;
		if (gap <= 40 && gap >= -style::ConvertScale(2)) {
			return true;
		}
	}
	return false;
}

rpl::producer<> MusicShownChanges() {
	return MusicChanges.events();
}

MusicButton::MusicButton(
	QWidget *parent,
	MusicButtonData data,
	Fn<void()> handler)
: RippleButton(parent, st::infoMusicButtonRipple)
, _noteSymbol(u"\u266B"_q + QChar(' '))
, _noteWidth(st::normalFont->width(_noteSymbol)) {
	updateData(std::move(data));
	setClickedCallback(std::move(handler));
	MusicFadeChanges() | rpl::on_next([=] {
		update();
	}, lifetime());
	// Mogram: считаем плашку сразу при создании, а не при показе: иначе
	// заливки уголков шапки появляются с задержкой. Скрытие (другая вкладка)
	// по-прежнему снимает учёт в hideEvent.
	setCounted(true);
}

MusicButton::~MusicButton() {
	setCounted(false);
}

// Mogram: шапка считает только видимые плашки, скрытая (другая вкладка,
// режим редактирования) не должна оставлять тёмные уголки под шапкой.
void MusicButton::setCounted(bool counted) {
	if (_counted == counted) {
		return;
	}
	_counted = counted;
	MusicCount += counted ? 1 : -1;
	if (counted) {
		MusicWidgets.push_back(this);
		// Mogram: старая плашка при уничтожении сбрасывает цвет полосы, а
		// новая его уже задала в конструкторе: без этого уголки «связки»
		// шапки красятся фоном списка и видны дырками.
		MusicBand = Ui::BlendColors(
			_overrideBg.value_or(QColor(st::boxDividerBg->c)),
			Qt::black,
			0.12);
	} else {
		MusicWidgets.erase(
			ranges::remove(MusicWidgets, not_null<QWidget*>(this)),
			end(MusicWidgets));
	}
	if (!MusicCount) {
		MusicBand = std::nullopt;
	}
	MusicChanges.fire({});
}

void MusicButton::showEvent(QShowEvent *e) {
	RippleButton::showEvent(e);
	setCounted(true);
}

void MusicButton::hideEvent(QHideEvent *e) {
	RippleButton::hideEvent(e);
	setCounted(false);
}

void MusicButton::updateData(MusicButtonData data) {
	const auto result = data.name.textWithEntities();
	const auto performerLength = result.entities.empty()
		? 0
		: int(result.entities.front().length());
	_performer.setText(
		st::semiboldTextStyle,
		result.text.mid(0, performerLength));
	_title.setText(
		st::defaultTextStyle,
		result.text.mid(performerLength, result.text.size()));
	setAccessibleName(result.text);
	update();
}

void MusicButton::setOverrideBg(std::optional<QColor> color) {
	_overrideBg = color;
	MusicBand = Ui::BlendColors(
		color.value_or(st::boxDividerBg->c),
		Qt::black,
		0.12);
	MusicChanges.fire({});
	update();
}

void MusicButton::visibleTopBottomUpdated(int visibleTop, int visibleBottom) {
	// Плашка уезжает при прокрутке: шапке нужно перерисоваться, чтобы
	// скруглить низ, когда плашка ушла из-под неё.
	if (std::abs(visibleTop - _lastVisibleTop) >= 2) {
		_lastVisibleTop = visibleTop;
		_visibleTop = std::max(visibleTop, 0);
		update();
		MusicFadeEvents.fire({});
	}
}

void MusicButton::setCovered(bool covered) {
	if (_covered == covered) {
		return;
	}
	_covered = covered;
	update();
}

void MusicButton::paintEvent(QPaintEvent *e) {
	if (_covered) {
		return;
	}
	auto p = QPainter(this);
	paintContent(p);
}

void MusicButton::paintContent(QPainter &p) {
	p.setOpacity(MusicFade());

	// Mogram: музыка продолжает шапку: шапка скруглена внизу и входит в
	// полосу, полоса скруглена внизу тем же радиусом; цвет плавно темнеет.
	constexpr auto kRadius = 32.;
	const auto detached = MusicDetached();
	// Отдельная таблетка: небольшой зазор от шапки и скругление со всех сторон.
	const auto gap = detached ? double(style::ConvertScale(6)) : 0.;
	const auto pill = QRectF(rect().marginsRemoved(QMargins(0, 0, 0, 0)))
		.adjusted(0, gap, 0, 0);
	auto path = QPainterPath();
	if (detached) {
		const auto radius = std::min(kRadius, pill.height() / 2.);
		path.addRoundedRect(pill, radius, radius);
	} else {
		path.addRoundedRect(
			pill.adjusted(0, -kRadius, 0, 0),
			kRadius,
			kRadius);
		path = path.intersected([&] {
			auto clip = QPainterPath();
			clip.addRect(pill);
			return clip;
		}());
	}
	{
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		const auto base = _overrideBg.value_or(QColor(st::boxDividerBg->c));
		// Ровная заливка: без градиента-«тени» под шапкой.
		p.setBrush(Ui::BlendColors(base, Qt::black, 0.12));
		p.drawPath(path);
	}
	p.setClipPath(path);
	const auto lightBackground = _overrideBg
		&& Ui::IsLightBackground(*_overrideBg);
	const auto rippleColor = !_overrideBg
		? std::nullopt
		: std::make_optional(anim::with_alpha(
			(lightBackground
				? QColor(Qt::black)
				: st::groupCallMembersFg->c),
			st::infoProfileTopBarBackdropRippleOpacity));
	paintRipple(p, QPoint(), rippleColor ? &*rippleColor : nullptr);

	const auto &icon = st::topicButtonArrow;
	const auto iconWidth = icon.width();
	const auto iconHeight = icon.height();

	const auto padding = st::infoMusicButtonPadding;
	const auto skip = st::normalFont->spacew;

	const auto titleWidth = _title.maxWidth();
	const auto performerWidth = _performer.maxWidth();
	const auto totalNeeded = titleWidth + performerWidth + skip;
	const auto availableWidth = width()
		- rect::m::sum::h(padding)
		- iconWidth
		- skip
		- _noteWidth;

	auto actualTitleWidth = 0;
	auto actualPerformerWidth = 0;
	if (totalNeeded <= availableWidth) {
		actualTitleWidth = titleWidth;
		actualPerformerWidth = performerWidth;
	} else {
		const auto ratio = float64(titleWidth) / totalNeeded;
		actualPerformerWidth = int(availableWidth * (1.0 - ratio));
		actualTitleWidth = availableWidth - actualPerformerWidth;
	}

	const auto totalContentWidth = _noteWidth
		+ actualPerformerWidth
		+ skip
		+ actualTitleWidth
		+ skip
		+ iconWidth;
	const auto centerX = width() / 2;
	const auto contentStartX = centerX - totalContentWidth / 2;
	const auto textTop = int(pill.top()
		+ (pill.height() - st::normalFont->height) / 2);

	p.setPen(!_overrideBg
		? st::windowBoldFg->c
		: lightBackground
		? QColor(Qt::black)
		: st::groupCallMembersFg->c);
	p.setFont(st::normalFont);
	p.drawText(contentStartX, textTop + st::normalFont->ascent, _noteSymbol);

	_performer.draw(p, {
		.position = { contentStartX + _noteWidth, textTop },
		.availableWidth = actualPerformerWidth,
		.now = crl::now(),
		.elisionLines = 1,
		.elisionMiddle = true,
	});

	p.setPen(!_overrideBg
		? st::windowSubTextFg->c
		: lightBackground
		? anim::with_alpha(
			QColor(Qt::black),
			st::groupCallVideoSubTextFg->c.alphaF())
		: st::groupCallVideoSubTextFg->c);
	_title.draw(p, {
		.position = QPoint(
			contentStartX + _noteWidth + actualPerformerWidth + skip,
			textTop),
		.availableWidth = actualTitleWidth,
		.now = crl::now(),
		.elisionLines = 1,
		.elisionMiddle = true,
	});

	const auto iconLeft = contentStartX
		+ _noteWidth
		+ actualPerformerWidth
		+ actualTitleWidth
		+ skip
		+ skip;
	const auto iconTop = int(pill.top() + (pill.height() - iconHeight) / 2);
	icon.paint(p, iconLeft, iconTop, iconWidth, p.pen().color());
}

MusicSticky::MusicSticky(QWidget *parent)
: RpWidget(parent) {
	setCursor(Qt::PointingHandCursor);
	StickyWidgets.push_back(this);
	hide();
}

MusicSticky::~MusicSticky() {
	StickyWidgets.erase(
		ranges::remove(StickyWidgets, not_null<QWidget*>(this)),
		end(StickyWidgets));
	if (const auto button = _button.data()) {
		button->setCovered(false);
	}
}

void MusicSticky::setButton(MusicButton *button) {
	if (_button.data() == button) {
		return;
	}
	if (const auto old = _button.data()) {
		old->setCovered(false);
	}
	_button = button;
}

void MusicSticky::paintEvent(QPaintEvent *e) {
	if (const auto button = _button.data()) {
		auto p = QPainter(this);
		button->paintContent(p);
	}
}

void MusicSticky::mousePressEvent(QMouseEvent *e) {
	_pressed = (e->button() == Qt::LeftButton);
}

void MusicSticky::mouseReleaseEvent(QMouseEvent *e) {
	const auto pressed = std::exchange(_pressed, false);
	if (pressed
		&& e->button() == Qt::LeftButton
		&& rect().contains(e->pos())) {
		if (const auto button = _button.data()) {
			button->clicked({}, Qt::LeftButton);
		}
	}
}

int MusicButton::resizeGetHeight(int newWidth) {
	const auto padding = st::infoMusicButtonPadding;
	const auto &font = st::defaultTextStyle.font;

	return padding.top() + font->height + padding.bottom();
}

} // namespace Info::Profile
