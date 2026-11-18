/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/ui/chat_search_empty.h"

#include "base/object_ptr.h"
#include "base/timer.h"
#include "ui/effects/animations.h"
#include "ui/painter.h"
#include "ui/widgets/labels.h"
#include "styles/style_dialogs.h"
#include "styles/style_widgets.h"

#include <QtGui/QLinearGradient>
#include <algorithm>
#include <cmath>

namespace Dialogs {
namespace {

// Mogram: своя векторная лупа вместо lottie-стикера. Рисуется в любом
// масштабе чётко, при показе один раз «осматривается» и замирает.
class Magnifier final : public Ui::RpWidget {
public:
	Magnifier(QWidget *parent, bool noResults, int size)
	: RpWidget(parent)
	, _noResults(noResults) {
		resize(size, size);
		setAttribute(Qt::WA_TransparentForMouseEvents);
		// Лупа «осматривается» снова и снова, пока видна.
		_repeat.setCallback([=] {
			if (isVisible()) {
				play();
			}
		});
		_repeat.callEach(3600);
	}

	void play() {
		_scan.stop();
		_scan.start([=] { update(); }, 0., 1., 2600, anim::sineInOut);
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		auto hq = PainterHighQualityEnabler(p);
		const auto side = float64(std::min(width(), height()));
		const auto t = _scan.value(1.);
		// Затухающее круговое движение линзы.
		// Размах нарастает и затухает плавно: ни резкого старта, ни остановки.
		const auto swing = std::sin(3.14159265358979323846 * t) * side * 0.07;
		const auto angle = t * 2. * 3.14159265358979323846 * 2.;
		const auto shift = QPointF(
			std::sin(angle) * swing,
			std::cos(angle) * swing - swing);
		const auto origin = QPointF(
			(width() - side) / 2.,
			(height() - side) / 2.);
		const auto radius = side * 0.27;
		const auto center = origin
			+ QPointF(side * 0.42, side * 0.42)
			+ shift;
		const auto ring = side * 0.07;
		const auto base = QColor(st::windowSubTextFg->c);
		auto light = base.lighter(125);
		auto dark = base.darker(135);

		// Ручка.
		const auto dir = QPointF(0.7071, 0.7071);
		const auto from = center + dir * (radius + ring * 0.2);
		const auto to = center + dir * (radius + side * 0.30);
		auto handle = QLinearGradient(from, to);
		handle.setColorAt(0., light);
		handle.setColorAt(1., dark);
		p.setPen(QPen(QBrush(handle), side * 0.1, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(from, to);

		// Стекло: слабая заливка и блик.
		auto glass = base;
		glass.setAlphaF(0.12);
		p.setPen(Qt::NoPen);
		p.setBrush(glass);
		p.drawEllipse(center, radius, radius);

		// Оправа.
		auto rim = QLinearGradient(
			center - QPointF(radius, radius),
			center + QPointF(radius, radius));
		rim.setColorAt(0., light);
		rim.setColorAt(1., dark);
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(QBrush(rim), ring));
		p.drawEllipse(center, radius, radius);

		auto shine = QColor(255, 255, 255, 90);
		p.setPen(QPen(shine, ring * 0.5, Qt::SolidLine, Qt::RoundCap));
		const auto inner = radius * 0.62;
		p.drawArc(
			QRectF(center.x() - inner, center.y() - inner, inner * 2, inner * 2),
			110 * 16,
			60 * 16);

		if (_noResults) {
			// Крестик внутри линзы.
			const auto cross = radius * 0.34;
			p.setPen(QPen(base, ring * 0.7, Qt::SolidLine, Qt::RoundCap));
			p.drawLine(
				center + QPointF(-cross, -cross),
				center + QPointF(cross, cross));
			p.drawLine(
				center + QPointF(-cross, cross),
				center + QPointF(cross, -cross));
		}
	}

private:
	Ui::Animations::Simple _scan;
	base::Timer _repeat;
	bool _noResults = false;

};

} // namespace

SearchEmpty::SearchEmpty(
	QWidget *parent,
	Icon icon,
	rpl::producer<TextWithEntities> text)
: RpWidget(parent) {
	setup(icon, std::move(text));
}

void SearchEmpty::setMinimalHeight(int minimalHeight) {
	const auto minimal = st::recentPeersEmptyHeightMin;
	resize(width(), std::max(minimalHeight, minimal));
}

void SearchEmpty::setup(Icon icon, rpl::producer<TextWithEntities> text) {
	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		this,
		std::move(text),
		st::defaultPeerListAbout);
	const auto size = st::recentPeersEmptySize;
	const auto animated = Ui::CreateChild<Magnifier>(
		this,
		(icon == Icon::NoResults),
		size);
	animated->show();
	animated->play();

	sizeValue() | rpl::on_next([=](QSize size) {
		const auto padding = st::recentPeersEmptyMargin;
		const auto paddings = padding.left() + padding.right();
		label->resizeToWidth(size.width() - paddings);
		const auto x = (size.width() - animated->width()) / 2;
		const auto y = (size.height() - animated->height()) / 3;
		const auto top = y + animated->height() + st::recentPeersEmptySkip;
		const auto sub = std::max(top + label->height() - size.height(), 0);
		animated->move(x, y - sub);
		label->move((size.width() - label->width()) / 2, top - sub);
	}, lifetime());

	label->setClickHandlerFilter([=](
			const ClickHandlerPtr &handler,
			Qt::MouseButton) {
		_handlerActivated.fire_copy(handler);
		return false;
	});

	_animate = [=] {
		animated->play();
	};
}

void SearchEmpty::animate() {
	if (const auto onstack = _animate) {
		onstack();
	}
}

} // namespace Dialogs
