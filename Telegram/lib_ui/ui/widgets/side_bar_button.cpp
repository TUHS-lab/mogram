// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#include "ui/widgets/side_bar_button.h"

#include "ui/effects/animations.h"
#include "ui/effects/animation_value.h"
#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "styles/style_widgets.h"

#include <QtGui/QtEvents>
#include <QAccessible>

namespace Ui {
namespace {

constexpr auto kMaxLabelLines = 3;
constexpr auto kPremiumLockedOpacity = 0.6;

// Mogram: одна общая пилюля выбранной вкладки на весь столбец. При смене
// вкладки она плавно перетекает к новой кнопке, а не гаснет и зажигается.
class PillState final : public QObject {
public:
	explicit PillState(QWidget *parent)
	: QObject(parent)
	, _parent(parent) {
		setObjectName(QStringLiteral("mog-pill"));
	}

	[[nodiscard]] static PillState *Find(QWidget *parent) {
		return static_cast<PillState*>(parent->findChild<QObject*>(
			QStringLiteral("mog-pill"),
			Qt::FindDirectChildrenOnly));
	}
	[[nodiscard]] static PillState *Ensure(QWidget *parent) {
		const auto found = Find(parent);
		return found ? found : new PillState(parent);
	}

	void moveTo(const QRect &target) {
		if (target == _target) {
			return;
		}
		if (_target.isNull() || anim::Disabled()) {
			_target = target;
			_from = target;
			_animation.stop();
			refresh();
			return;
		}
		_from = current();
		_target = target;
		_animation.start(
			[=] { refresh(); },
			0.,
			1.,
			280,
			anim::easeOutCubic);
	}

	[[nodiscard]] QRect target() const {
		return _target;
	}

	[[nodiscard]] QRect current() const {
		const auto t = _animation.value(1.);
		const auto lerp = [&](int a, int b) {
			return int(std::lround(a + (b - a) * t));
		};
		return QRect(
			lerp(_from.x(), _target.x()),
			lerp(_from.y(), _target.y()),
			lerp(_from.width(), _target.width()),
			lerp(_from.height(), _target.height()));
	}

private:
	void refresh() {
		for (const auto child : _parent->children()) {
			if (const auto button = dynamic_cast<SideBarButton*>(child)) {
				button->update();
			}
		}
	}

	QWidget *_parent = nullptr;
	QRect _from;
	QRect _target;
	Ui::Animations::Simple _animation;

};

} // namespace

SideBarButton::SideBarButton(
	not_null<QWidget*> parent,
	const TextWithEntities &title,
	const style::SideBarButton &st,
	Text::MarkedContext context,
	Fn<bool()> paused)
: RippleButton(parent, st.ripple)
, _st(st)
, _text(_st.minTextWidth)
, _paused(paused)
, _context(std::move(context)) {
	_context.repaint = [this] { update(); };
	_text.setMarkedText(
		_st.style,
		title,
		kMarkupTextOptions,
		_context);
	setAttribute(Qt::WA_OpaquePaintEvent);

	style::PaletteChanged(
	) | rpl::on_next([this] {
		_iconCache = _iconCacheActive = QImage();
		_lock.iconCache = _lock.iconCacheActive = QImage();
		update();
	}, lifetime());
}

void SideBarButton::setActive(bool active) {
	if (_active == active) {
		return;
	}
	_active = active;
	if (active && parentWidget()) {
		PillState::Ensure(parentWidget())->moveTo(
			geometry().marginsRemoved({ 4, 2, 4, 2 }));
	}
	if (isPageTab()) {
		// Announce selection via a dedicated selection event (the Windows
		// bridge maps these to UIA_SelectionItem_ElementSelected); a generic
		// state-change event is ignored for the selected state.
		auto event = QAccessibleEvent(
			this,
			active ? QAccessible::SelectionAdd : QAccessible::SelectionRemove);
		QAccessible::updateAccessibility(&event);
	}
	update();
}

AccessibilityState SideBarButton::accessibilityState() const {
	// Merge the base state so plain buttons keep reporting `pressed`. A tab
	// exposes the active one as selected - persistently, independent of
	// keyboard focus. A locked (premium) folder stays a focusable, invokable
	// tab but can never become current, so it isn't selectable.
	auto state = RippleButton::accessibilityState();
	if (isPageTab()) {
		state.selectable = !_lock.locked;
		state.selected = _active;
	}
	return state;
}

void SideBarButton::setBadge(const QString &badge, bool muted) {
	if (_badge.toString() == badge && _badgeMuted == muted) {
		return;
	}
	_badge.setText(_st.badgeStyle, badge);
	_badgeMuted = muted;
	const auto width = badge.isEmpty()
		? 0
		: std::max(_st.badgeHeight, _badge.maxWidth() + 2 * _st.badgeSkip);
	if (_iconCacheBadgeWidth != width) {
		_iconCacheBadgeWidth = width;
		_iconCache = _iconCacheActive = QImage();
	}
	accessibilityNameChanged();
	update();
}

void SideBarButton::setIconOverride(
		const style::icon *iconOverride,
		const style::icon *iconOverrideActive) {
	_iconOverride = iconOverride;
	_iconOverrideActive = iconOverrideActive;
	update();
}

void SideBarButton::setLocked(bool locked) {
	if (_lock.locked == locked) {
		return;
	}
	_lock.locked = locked;
	_iconCache = _iconCacheActive = QImage();
	const auto charFiller = QChar('l');
	const auto count = std::ceil(st::sideBarButtonLockSize.width()
		/ float(_st.style.font->width(charFiller)));
	const auto filler = QString().fill(charFiller, count);
	auto result = TextWithEntities();
	if (_lock.locked) {
		result.append(filler);
	}
	const auto len = _text.length();
	result.append(_text.toTextWithEntities({
		ushort(_lock.locked ? 0 : count),
		ushort(len),
	}));
	_text.setMarkedText(_st.style, result, kMarkupTextOptions, _context);
	update();
}

bool SideBarButton::locked() const {
	return _lock.locked;
}

void SideBarButton::setShowIcon(bool shown) {
	if (_showIcon == shown) {
		return;
	}
	_showIcon = shown;
	update();
}

void SideBarButton::setShowText(bool shown) {
	if (_showText == shown) {
		return;
	}
	_showText = shown;
	_iconCache = _iconCacheActive = QImage();
	if (const auto current = width()) {
		resizeToWidth(current);
	}
	update();
}

int SideBarButton::resizeGetHeight(int newWidth) {
	auto result = _st.minHeight;
	if (!_showText) {
		return result;
	}
	const auto text = std::min(
		_text.countHeight(newWidth - _st.textSkip * 2),
		_st.style.font->height * kMaxLabelLines);
	const auto add = text - _st.style.font->height;
	return result + std::max(add, 0);
}

void SideBarButton::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto clip = e->rect();

	const auto &bg = _active ? _st.textBgActive : _st.textBg;
	// Mogram: вкладка-пилюля вместо прямоугольника.
	p.fillRect(clip, _st.textBg);
	if (const auto parent = parentWidget()) {
		if (_active) {
			// Положение могло сдвинуться при перекомпоновке: подтягиваем пилюлю.
			PillState::Ensure(parent)->moveTo(
				geometry().marginsRemoved({ 4, 2, 4, 2 }));
		}
		// Пилюлю рисует каждая кнопка в своей части: пока она едет от старой
		// вкладки к новой, видна и над старой, иначе движение обрывалось бы.
		if (const auto state = PillState::Find(parent)) {
			const auto pill = state->current().translated(-pos());
			if (pill.intersects(rect())) {
				auto hq = PainterHighQualityEnabler(p);
				p.setPen(Qt::NoPen);
				p.setBrush(_st.textBgActive);
				p.drawRoundedRect(pill, 16, 16);
			}
		}
	}

	// Mogram: вспышки при нажатии на плитку нет.

	if (_lock.locked) {
		p.setOpacity(kPremiumLockedOpacity);
	}

	if (_showIcon) {
		const auto &icon = computeIcon();
		const auto x = iconLeft();
		const auto y = iconTop();
		if (_st.iconPosition.x() < 0) {
			// Mogram: значок папки крупнее, рисуется из кэша, выровнен по центру.
			validateIconCache();
			const auto &cache = _active ? _iconCacheActive : _iconCache;
			const auto ratio = style::DevicePixelRatio();
			const auto dx = (cache.width() / ratio - icon.width()) / 2;
			const auto dy = (cache.height() / ratio - icon.height()) / 2;
			p.drawImage(x - dx, y - dy, cache);
		} else {
			icon.paint(p, x, y, width());
		}
	}
	if (_showText) {
		p.setPen(_active ? _st.textFgActive : _st.textFg);
		_text.draw(p, {
			.position = { _st.textSkip, _st.textTop },
			.availableWidth = (width() - 2 * _st.textSkip),
			.align = style::al_top,
			.pausedEmoji = _paused && _paused(),
			.elisionLines = kMaxLabelLines,
		});
	}

	if (_iconCacheBadgeWidth) {
		const auto desiredLeft = width() / 2 + _st.badgePosition.x();
		const auto x = std::min(
			desiredLeft,
			width() - _iconCacheBadgeWidth - st::defaultScrollArea.width);
		const auto y = _st.badgePosition.y();

		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(_active
			? (_badgeMuted ? _st.badgeBgMutedActive : _st.badgeBgActive)
			: (_badgeMuted ? _st.badgeBgMuted : _st.badgeBg));
		const auto r = _st.badgeHeight / 2;
		p.drawRoundedRect(x, y, _iconCacheBadgeWidth, _st.badgeHeight, r, r);

		p.setPen(_st.badgeFg);
		_badge.draw(
			p,
			x + (_iconCacheBadgeWidth - _badge.maxWidth()) / 2,
			y + (_st.badgeHeight - _st.badgeStyle.font->height) / 2,
			width());
	}

	if (_lock.locked) {
		validateLockIconCache();

		const auto &icon = _active ? _lock.iconCacheActive : _lock.iconCache;
		const auto size = icon.size() / style::DevicePixelRatio();
		if (_showText) {
			const auto lineWidths = _text.countLineWidths(
				width() - 2 * _st.textSkip,
				{ .reserve = kMaxLabelLines });
			if (lineWidths.empty()) {
				return;
			}
			p.translate(
				(width() - lineWidths.front()) / 2.,
				_st.textTop + (_st.style.font->height - size.height()) / 2.);
			p.setOpacity(1.);
			p.fillRect(QRect(QPoint(), size), bg);
			p.setOpacity(kPremiumLockedOpacity);
			p.translate(-_st.style.font->spacew / 2., 0);
		} else {
			const auto &folder = computeIcon();
			const auto shift = st::sideBarButtonLockCornerShift;
			p.translate(
				iconLeft() + folder.width() - shift - size.width() / 2,
				iconTop() + folder.height() - shift - size.height() / 2);
		}
		p.drawImage(0, 0, icon);
	}
}

const style::icon &SideBarButton::computeIcon() const {
	return _active
		? (_iconOverrideActive
			? *_iconOverrideActive
			: !_st.iconActive.empty()
			? _st.iconActive
			: _iconOverride
			? *_iconOverride
			: _st.icon)
		: _iconOverride
		? *_iconOverride
		: _st.icon;
}

int SideBarButton::iconLeft() const {
	return (_st.iconPosition.x() < 0)
		? (width() - computeIcon().width()) / 2
		: _st.iconPosition.x();
}

int SideBarButton::iconTop() const {
	return (_st.iconPosition.y() < 0)
		? (height() - computeIcon().height()) / 2
		: _st.iconPosition.y();
}

void SideBarButton::validateIconCache() {
	Expects(_st.iconPosition.x() < 0);

	if (!(_active ? _iconCacheActive : _iconCache).isNull()) {
		return;
	}
	const auto &icon = computeIcon();
	// Значок папки увеличен: кэш больше значка, края делят поровну.
	constexpr auto kIconScale = 1.1;
	const auto scaled = QSize(
		int(std::ceil(icon.width() * kIconScale)),
		int(std::ceil(icon.height() * kIconScale)));
	const auto extraX = (scaled.width() - icon.width()) / 2;
	const auto extraY = (scaled.height() - icon.height()) / 2;
	auto image = QImage(
		scaled * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(style::DevicePixelRatio());
	image.fill(Qt::transparent);
	{
		auto p = QPainter(&image);
		{
			auto hq = PainterHighQualityEnabler(p);
			p.setRenderHint(QPainter::SmoothPixmapTransform);
			p.scale(
				scaled.width() / float64(icon.width()),
				scaled.height() / float64(icon.height()));
			icon.paint(p, 0, 0, icon.width());
			p.resetTransform();
		}
		p.setCompositionMode(QPainter::CompositionMode_Source);
		auto hq = PainterHighQualityEnabler(p);
		if (_iconCacheBadgeWidth) {
			p.setBrush(Qt::transparent);
			auto pen = QPen(Qt::transparent);
			pen.setWidth(2 * _st.badgeStroke);
			p.setPen(pen);
			const auto desiredLeft = (width() / 2) + _st.badgePosition.x();
			const auto pillLeft = std::min(
				desiredLeft,
				width() - _iconCacheBadgeWidth - st::defaultScrollArea.width);
			const auto x = pillLeft - iconLeft() + extraX;
			const auto y = _st.badgePosition.y() - iconTop() + extraY;
			const auto r = _st.badgeHeight / 2.;
			p.drawRoundedRect(
				x,
				y,
				_iconCacheBadgeWidth,
				_st.badgeHeight,
				r,
				r);
		}
		if (_lock.locked && !_showText) {
			p.setPen(Qt::NoPen);
			p.setBrush(Qt::transparent);
			const auto shift = st::sideBarButtonLockCornerShift;
			const auto radius = (st::sideBarButtonLockSize.height() / 2)
				+ _st.badgeStroke;
			p.drawEllipse(
				QPoint(scaled.width() - shift - extraX, scaled.height() - shift - extraY),
				radius,
				radius);
		}
	}
	(_active ? _iconCacheActive : _iconCache) = std::move(image);
}

void SideBarButton::validateLockIconCache() {
	if (!(_active ? _lock.iconCacheActive : _lock.iconCache).isNull()) {
		return;
	}
	(_active ? _lock.iconCacheActive : _lock.iconCache)
		= SideBarLockIcon(_st.textFg);
}

QImage SideBarLockIcon(const style::color &fg) {
	const auto &size = st::sideBarButtonLockSize;
	const auto arcPen = QPen(
		fg,
		// Use a divider to get 1.5.
		st::sideBarButtonLockPenWidth
			/ float64(st::sideBarButtonLockPenWidthDivider),
		Qt::SolidLine,
		Qt::SquareCap,
		Qt::RoundJoin);
	auto image = QImage(
		size * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(style::DevicePixelRatio());
	image.fill(Qt::transparent);
	{
		auto p = QPainter(&image);
		auto hq = PainterHighQualityEnabler(p);

		const auto &arcOffset = st::sideBarButtonLockArcOffset;
		const auto arcWidth = size.width() - arcOffset * 2;
		const auto &arcHeight = st::sideBarButtonLockArcHeight;

		const auto blockRectWidth = size.width();
		const auto blockRectHeight = st::sideBarButtonLockBlockHeight;
		const auto blockRectTop = size.height() - blockRectHeight;

		const auto blockRect = QRectF(
			(size.width() - blockRectWidth) / 2,
			blockRectTop,
			blockRectWidth,
			blockRectHeight);
		const auto lineHeight = -(blockRect.y() - arcHeight)
			+ arcPen.width() / 2.;

		p.setPen(Qt::NoPen);
		p.setBrush(fg);
		{
			p.drawRoundedRect(blockRect, 2, 2);
		}

		p.translate(size.width() - arcOffset, blockRect.y());

		p.setPen(arcPen);
		const auto rLine = QLineF(0, 0, 0, lineHeight);
		const auto lLine = rLine.translated(-arcWidth, 0);
		p.drawLine(rLine);
		p.drawLine(lLine);

		p.drawArc(
			-arcWidth,
			-arcHeight - arcPen.width() / 2.,
			arcWidth,
			arcHeight * 2,
			0,
			180 * 16);
	}
	return image;
}

} // namespace Ui
