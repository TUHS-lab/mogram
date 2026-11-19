/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/info_content_widget.h"
#include "ui/painter.h"

#include "api/api_who_reacted.h"
#include "boxes/peer_list_box.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_forum_topic.h"
#include "data/data_forum.h"
#include "data/data_saved_messages.h"
#include "data/data_user.h"
#include "info/profile/info_profile_widget.h"
#include "info/media/info_media_widget.h"
#include "info/common_groups/info_common_groups_widget.h"
#include "info/peer_gifts/info_peer_gifts_common.h"
#include "info/saved/info_saved_music_common.h"
#include "info/stories/info_stories_common.h"
#include "info/info_layer_widget.h"
#include "info/info_section_widget.h"
#include "info/info_controller.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "menu/menu_send.h"
#include "ui/controls/swipe_handler.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/search_field_controller.h"
#include "ui/ui_utility.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_info.h"

#include <QtGui/QPainterPath>
#include "styles/style_profile.h"
#include "styles/style_dialogs.h"

namespace Info {
namespace {

class FlexibleFiller final : public Ui::RpWidget {
public:
	using RpWidget::RpWidget;

	void setTargetWidget(base::unique_qptr<RpWidget> widget);

private:
	void visibleTopBottomUpdated(int visibleTop, int visibleBottom) override;

	base::unique_qptr<RpWidget> _target;

};

void FlexibleFiller::setTargetWidget(base::unique_qptr<RpWidget> widget) {
	Expects(!_target);

	_target = std::move(widget);
}

void FlexibleFiller::visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) {
	if (const auto raw = _target.get()) {
		raw->setVisibleTopBottom(visibleTop, visibleBottom);
	}
}

} // namespace

ContentWidget::ContentWidget(
	QWidget *parent,
	not_null<Controller*> controller)
: RpWidget(parent)
, _controller(controller)
, _scroll(
	this,
	(_controller->wrap() == Wrap::Search
		? st::infoSharedMediaScroll
		: st::defaultScrollArea)) {
	using namespace rpl::mappers;

	setAttribute(Qt::WA_OpaquePaintEvent);
	_controller->wrapValue(
	) | rpl::on_next([this](Wrap value) {
		if (value != Wrap::Layer) {
			applyAdditionalScroll(0);
		}
		// Mogram: у окна один фон сверху, посередине и снизу.
		_bg = st::dialogsBg;
		// Mogram: в окне углы скруглены, так что подложка не может быть квадратной.
		setAttribute(Qt::WA_OpaquePaintEvent, value != Wrap::Layer);
		update();
	}, lifetime());
	if (_controller->section().type() != Section::Type::Profile) {
		rpl::combine(
			_controller->wrapValue(),
			_controller->searchEnabledByContent(),
			(_1 == Wrap::Layer) && _2
		) | rpl::distinct_until_changed(
		) | rpl::on_next([this](bool shown) {
			refreshSearchField(shown);
		}, lifetime());
	}
	rpl::merge(
		_scrollTopSkip.changes(),
		_scrollBottomSkip.changes()
	) | rpl::on_next([this] {
		updateControlsGeometry();
		if (_innerWrap && _overlapTopSkip) {
			updateInnerPadding();
		}
	}, lifetime());

	_scroll->scrollTopChanges(
	) | rpl::on_next([this] {
		if (!_applyingScrollTopRestore) {
			_scrollTopRestore = std::nullopt;
		}
	}, lifetime());

	// Mogram: у краёв прокручиваемого окна мягкий переход в фон: сверху, когда
	// есть что прокрутить вверх, снизу, когда есть что прокрутить вниз.
	{
		const auto makeFade = [=](bool top) {
			const auto fade = Ui::CreateChild<Ui::RpWidget>(this);
			fade->setAttribute(Qt::WA_TransparentForMouseEvents);
			fade->paintRequest(
			) | rpl::on_next([=] {
				auto p = QPainter(fade);
				// Нижний переход не торчит квадратом за скруглёнными
				// нижними углами слоя (подложка слоя ниже на 10 пикселей).
				if (!top && _controller->wrap() == Wrap::Layer) {
					const auto radius = style::ConvertScale(28);
					const auto extra = style::ConvertScale(14);
					auto path = QPainterPath();
					path.addRoundedRect(
						QRectF(
							0,
							-2 * radius,
							fade->width(),
							fade->height() + extra + 2 * radius),
						radius,
						radius);
					p.setRenderHint(QPainter::Antialiasing);
					p.setClipPath(path);
				}
				auto gradient = QLinearGradient(0, 0, 0, fade->height());
				for (auto i = 0; i <= 8; ++i) {
					const auto t = i / 8.;
					const auto eased = t * t * (3. - 2. * t);
					// Под шапкой профиля переход в цвет шапки, иначе в цвет окна.
					const auto profile = (_controller->section().type()
						== Section::Type::Profile);
					auto color = (top && profile)
						? st::boxDividerBg->c
						: st::dialogsBg->c;
					color.setAlphaF(top ? (1. - eased) : (0.72 * eased));
					gradient.setColorAt(t, color);
				}
				p.fillRect(fade->rect(), gradient);
				// Карточки внизу заканчиваются скруглёнными углами, а не
				// острыми: снаружи воображаемой карточки (поля 10 пикселей,
				// радиус как у карточек) доливаем цвет окна.
				// Mogram: рамка из карточных уголков только у профиля; в списках
				// медиа (видео, фото) плитки идут до низа без широкой рамки.
				if (!top && _controller->section().type() == Section::Type::Profile) {
					const auto side = 10;
					const auto radius = style::ConvertScale(25);
					auto outside = QPainterPath();
					outside.addRect(QRectF(fade->rect()));
					auto card = QPainterPath();
					card.addRoundedRect(
						QRectF(
							side,
							-4 * radius,
							fade->width() - 2 * side,
							fade->height() + 4 * radius),
						radius,
						radius);
					// Только внутри слоя: за его скруглёнными углами ничего.
					auto inner = QPainterPath();
					if (_controller->wrap() == Wrap::Layer) {
						const auto layerRadius = style::ConvertScale(14);
						inner.addRoundedRect(
							QRectF(
								0,
								-4 * layerRadius,
								fade->width(),
								fade->height()
									+ style::ConvertScale(14)
									+ 4 * layerRadius),
							layerRadius,
							layerRadius);
					} else {
						inner.addRect(QRectF(fade->rect()));
					}
					p.setRenderHint(QPainter::Antialiasing);
					p.fillPath(
						outside.subtracted(card).intersected(inner),
						st::dialogsBg);
				}
			}, fade->lifetime());
			fade->hide();
			return fade;
		};
		const auto topFade = makeFade(true);
		const auto bottomFade = makeFade(false);
		const auto place = [=] {
			const auto layer = (_controller->wrap() == Wrap::Layer);
			const auto height = style::ConvertScale(16);
			// Высота перехода не меньше радиуса карточки, чтобы скругление было целым.
			const auto bottomHeight = style::ConvertScale(20);
			const auto g = _scroll->geometry();
			const auto profile = (_controller->section().type()
				== Section::Type::Profile);
			// У профиля переход уже шапки: он повторяет её скруглённые края,
			// а не торчит квадратом за уголками.
			const auto inset = profile ? style::ConvertScale(20) : 0;
			topFade->setGeometry(
				g.x() + inset,
				g.y(),
				g.width() - 2 * inset,
				height);
			const auto margin = style::ConvertScale(6);
			// Под шапкой профиля тени нет.
			// Верхний переход: когда содержимое ушло под шапку окна.
			topFade->setVisible(
				layer && !profile && (_scroll->scrollTop() > margin));
			// Переход лежит под шапкой, чтобы не закрывать её.
			if (_fadeUnder) {
				topFade->stackUnder(_fadeUnder.data());
			} else {
				topFade->raise();
			}
			bottomFade->setGeometry(
				g.x(),
				g.y() + g.height() - bottomHeight,
				g.width(),
				bottomHeight);
			// Снизу переход только если действительно есть что прокручивать.
			bottomFade->setVisible(
				(layer || (_controller->wrap() == Wrap::Side))
				&& ((_scroll->scrollTopMax() - _scroll->scrollTop())
					> style::ConvertScale(40)));
			bottomFade->raise();
		};
		rpl::merge(
			_scroll->scrollTopChanges() | rpl::to_empty,
			_scroll->heightValue() | rpl::to_empty,
			_scroll->scrollTopValue() | rpl::to_empty
		) | rpl::on_next(place, lifetime());
		_placeFades = place;
	}
}

void ContentWidget::resizeEvent(QResizeEvent *e) {
	updateControlsGeometry();
}

void ContentWidget::updateControlsGeometry() {
	if (!_innerWrap) {
		return;
	}
	_innerWrap->resizeToWidth(width());

	auto scrollGeometry = rect().marginsRemoved(
		{
			0,
			_overlapTopSkip ? 0 : _scrollTopSkip.current(),
			0,
			_scrollBottomSkip.current(),
		});
	if (_scroll->geometry() != scrollGeometry) {
		_scroll->setGeometry(scrollGeometry);
	}
	// Mogram: прокрутка заходит под шапку; её верхние пиксели не видны, но у
	// скруглённых углов шапки выглядывал серый прямоугольник. Срезаем верх.
	if (_overlapTopSkip) {
		const auto cut = std::min(
			style::ConvertScale(14),
			_scroll->height());
		_scroll->setMask(QRegion(
			0,
			cut,
			_scroll->width(),
			std::max(_scroll->height() - cut, 0)));
	}

	if (!_scroll->isHidden()) {
		auto scrollTop = _scroll->scrollTop();
		_innerWrap->setVisibleTopBottom(
			scrollTop,
			scrollTop + _scroll->height());
	}
	if (_placeFades) {
		_placeFades();
	}
}

std::shared_ptr<ContentMemento> ContentWidget::createMemento() {
	auto result = doCreateMemento();
	_controller->saveSearchState(result.get());
	return result;
}

void ContentWidget::setFadeUnder(QWidget *widget) {
	_fadeUnder = widget;
	if (_placeFades) {
		_placeFades();
	}
}

void ContentWidget::setIsStackBottom(bool isStackBottom) {
	_isStackBottom = isStackBottom;
}

bool ContentWidget::isStackBottom() const {
	return _isStackBottom;
}

void ContentWidget::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	if (_controller->wrap() == Wrap::Layer) {
		// Скруглённая подложка сверху (32), снизу квадратная: низ скругляет слой.
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(_bg);
		// Подложка скруглена внизу (радиус как у окна); верх прямой под шапкой
		// окна, иначе в углах зазор, и скруглён, если шапки нет.
		const auto radius = style::ConvertScale(28);
		const auto top = (y() > 0) ? -radius : 0;
		p.drawRoundedRect(
			QRectF(rect()).adjusted(0, top, 0, 0),
			radius,
			radius);
		return;
	}
	// Mogram: скруглённая шапка профиля не закрашивает углы подложки.
	if (_paintPadding.isNull()
		|| _controller->section().type() == Section::Type::Profile) {
		p.fillRect(e->rect(), _bg);
	} else {
		const auto &r = e->rect();
		const auto padding = QMargins(
			0,
			std::min(0, (r.top() - _paintPadding.top())),
			0,
			std::min(0, (r.bottom() - _paintPadding.bottom())));
		p.fillRect(r + padding, _bg);
	}
}

Ui::RpWidget *ContentWidget::doSetInnerWidget(
		object_ptr<RpWidget> inner) {
	using namespace rpl::mappers;

	_innerWrap = _scroll->setOwnedWidget(
		object_ptr<Ui::PaddingWrap<Ui::RpWidget>>(
			this,
			std::move(inner),
			_innerWrap ? _innerWrap->padding() : style::margins()));
	_innerWrap->move(0, 0);

	setupSwipeHandler(_innerWrap);

	rpl::combine(
		_scroll->scrollTopValue(),
		_scroll->heightValue(),
		_innerWrap->entity()->desiredHeightValue(),
		tuple(_1, _1 + _2, _3)
	) | rpl::on_next([this](
			int top,
			int bottom,
			int desired) {
		_innerDesiredHeight = desired;
		_innerWrap->setVisibleTopBottom(top, bottom);
		_scrollTillBottomChanges.fire_copy(
			std::max(desired + _innerTopReserve + topOverlap() - bottom, 0));
	}, _innerWrap->lifetime());

	rpl::merge(
		_scroll->heightValue() | rpl::to_empty,
		_innerWrap->heightValue() | rpl::to_empty
	) | rpl::on_next([=] {
		applyScrollTopRestore();
	}, _innerWrap->lifetime());

	rpl::combine(
		_scroll->heightValue(),
		_innerWrap->entity()->heightValue(),
		_controller->wrapValue()
	) | rpl::on_next([=](
			int scrollHeight,
			int innerHeight,
			Wrap wrap) {
		const auto added = (wrap == Wrap::Layer)
			? 0
			: std::max(
				scrollHeight - innerHeight - _innerTopReserve - topOverlap(),
				0);
		if (_addedHeight != added) {
			_addedHeight = added;
			updateInnerPadding();
		}
	}, _innerWrap->lifetime());
	updateInnerPadding();

	return _innerWrap->entity();
}

Ui::RpWidget *ContentWidget::doSetupFlexibleInnerWidget(
		object_ptr<Ui::RpWidget> inner,
		FlexibleScrollData &flexibleScroll,
		Fn<void(Ui::RpWidget*)> customSetup) {
	const auto filler = setInnerWidget(object_ptr<FlexibleFiller>(this));
	filler->resize(1, 1);

	flexibleScroll.contentHeightValue.events(
	) | rpl::on_next([=](int h) {
		filler->resize(filler->width(), h);
	}, filler->lifetime());

	filler->widthValue(
	) | rpl::start_to_stream(
		flexibleScroll.fillerWidthValue,
		filler->lifetime());

	if (customSetup) {
		customSetup(filler);
	}

	// ScrollArea -> PaddingWrap -> RpWidget.
	const auto result = inner.release();
	result->setParent(filler->parentWidget()->parentWidget());
	result->raise();
	filler->setTargetWidget(base::unique_qptr<Ui::RpWidget>(result));

	return result;
}

int ContentWidget::scrollTillBottom(int forHeight) const {
	const auto scrollHeight = forHeight
		- (_overlapTopSkip ? 0 : _scrollTopSkip.current())
		- _scrollBottomSkip.current();
	const auto scrollBottom = _scroll->scrollTop() + scrollHeight;
	const auto desired = _innerDesiredHeight
		+ _innerTopReserve
		+ topOverlap();
	return std::max(desired - scrollBottom, 0);
}

rpl::producer<int> ContentWidget::scrollTillBottomChanges() const {
	return _scrollTillBottomChanges.events();
}

void ContentWidget::setScrollTopSkip(int scrollTopSkip) {
	_scrollTopSkip = scrollTopSkip;
}

void ContentWidget::setScrollBottomSkip(int scrollBottomSkip) {
	_scrollBottomSkip = scrollBottomSkip;
}

rpl::producer<int> ContentWidget::scrollHeightValue() const {
	return _scroll->heightValue();
}

void ContentWidget::applyAdditionalScroll(int additionalScroll) {
	if (_additionalScroll != additionalScroll) {
		_additionalScroll = additionalScroll;
		if (_innerWrap) {
			updateInnerPadding();
		}
	}
}

void ContentWidget::updateInnerPadding() {
	const auto addedToBottom = std::max(_additionalScroll, _addedHeight);
	_innerWrap->setPadding({
		0,
		_innerTopReserve + topOverlap(),
		0,
		addedToBottom,
	});
}

void ContentWidget::setInnerTopReserve(int reserve) {
	if (_innerTopReserve != reserve) {
		_innerTopReserve = reserve;
		if (_innerWrap) {
			updateInnerPadding();
		}
	}
}

void ContentWidget::setupFlexibleRegularScroll(
		not_null<Ui::RpWidget*> inner,
		not_null<Ui::RpWidget*> pinnedToTop,
		bool abortSnapOnExternalScroll) {
	_overlapTopSkip = true;
	SetupFlexibleRegularScroll(
		_scroll.data(),
		inner,
		pinnedToTop,
		[=](int skip) { setScrollTopSkip(skip); },
		[=](int reserve) { setInnerTopReserve(reserve); },
		[=](QMargins padding) { setPaintPadding(padding); },
		[=](rpl::producer<not_null<QEvent*>> events) {
			setViewport(std::move(events));
		},
		abortSnapOnExternalScroll);
}

void ContentWidget::applyMaxVisibleHeight(int maxVisibleHeight) {
	if (_maxVisibleHeight != maxVisibleHeight) {
		_maxVisibleHeight = maxVisibleHeight;
		update();
	}
}

rpl::producer<int> ContentWidget::desiredHeightValue() const {
	using namespace rpl::mappers;
	return rpl::combine(
		_innerWrap->entity()->desiredHeightValue(),
		_scrollTopSkip.value(),
		_scrollBottomSkip.value()
	//) | rpl::map(_1 + _2 + _3);
	) | rpl::map([=](int desired, int, int) {
		return desired
			+ _innerTopReserve
			+ _scrollTopSkip.current()
			+ _scrollBottomSkip.current();
	});
}

rpl::producer<bool> ContentWidget::desiredShadowVisibility() const {
	using namespace rpl::mappers;
	return rpl::combine(
		_scroll->scrollTopValue(),
		_scrollTopSkip.value()
	) | rpl::map((_1 > 0) || (_2 > 0));
}

bool ContentWidget::hasTopBarShadow() const {
	return (_scroll->scrollTop() > 0);
}

void ContentWidget::setInnerFocus() {
	if (_searchField) {
		_searchField->setFocus();
	} else {
		_innerWrap->entity()->setFocus();
	}
}

int ContentWidget::scrollTopSave() const {
	return _scroll->scrollTop();
}

rpl::producer<int> ContentWidget::scrollTopValue() const {
	return _scroll->scrollTopValue();
}

void ContentWidget::scrollTopRestore(int scrollTop) {
	_scrollTopRestore = scrollTop;
	applyScrollTopRestore();
}

void ContentWidget::applyScrollTopRestore() {
	if (!_scrollTopRestore || _applyingScrollTopRestore) {
		return;
	}
	const auto top = *_scrollTopRestore;
	if (_scroll->scrollTopMax() >= top) {
		_scrollTopRestore = std::nullopt;
	}
	_applyingScrollTopRestore = true;
	_scroll->scrollToY(top);
	_applyingScrollTopRestore = false;
}

void ContentWidget::scrollTo(const Ui::ScrollToRequest &request) {
	_scroll->scrollTo(request);
}

bool ContentWidget::floatPlayerHandleWheelEvent(QEvent *e) {
	return _scroll->viewportEvent(e);
}

QRect ContentWidget::floatPlayerAvailableRect() const {
	return mapToGlobal(_scroll->geometry());
}

void ContentWidget::fillTopBarMenu(const Ui::Menu::MenuCallback &addAction) {
	const auto peer = _controller->key().peer();
	const auto topic = _controller->key().topic();
	const auto sublist = _controller->key().sublist();
	if (!peer && !topic) {
		return;
	}

	Window::FillDialogsEntryMenu(
		_controller->parentController(),
		Dialogs::EntryState{
			.key = (topic
				? Dialogs::Key{ topic }
				: sublist
				? Dialogs::Key{ sublist }
				: Dialogs::Key{ peer->owner().history(peer) }),
			.section = Dialogs::EntryState::Section::Profile,
		},
		addAction);
}

void ContentWidget::checkBeforeCloseByEscape(Fn<void()> close) {
	if (_searchField) {
		if (!_searchField->empty()) {
			_searchField->setText({});
		} else {
			close();
		}
	} else {
		close();
	}
}

rpl::producer<SelectedItems> ContentWidget::selectedListValue() const {
	return rpl::single(SelectedItems(Storage::SharedMediaType::Photo));
}

void ContentWidget::setPaintPadding(const style::margins &padding) {
	_paintPadding = padding;
}

void ContentWidget::setViewport(
		rpl::producer<not_null<QEvent*>> &&events) const {
	std::move(
		events
	) | rpl::on_next([=](not_null<QEvent*> e) {
		_scroll->viewportEvent(e);
	}, _scroll->lifetime());
}

auto ContentWidget::titleStories()
-> rpl::producer<Dialogs::Stories::Content> {
	return nullptr;
}

void ContentWidget::saveChanges(FnMut<void()> done) {
	done();
}

SendMenu::Details ContentWidget::sendMenuDetails() const {
	return {};
}

bool ContentWidget::processChosenSticker(ChatHelpers::FileChosen &&) {
	return false;
}

bool ContentWidget::processScrollKey(not_null<QKeyEvent*> e) {
	const auto key = e->key();
	const auto modifiers = e->modifiers()
		& ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
	const auto scrollKey = (key == Qt::Key_Up)
		|| (key == Qt::Key_Down)
		|| (key == Qt::Key_PageUp)
		|| (key == Qt::Key_PageDown);
	if ((modifiers != Qt::NoModifier)
		|| !scrollKey
		|| _scroll->isHidden()) {
		return false;
	}
	_scroll->keyPressEvent(e);
	return true;
}

void ContentWidget::refreshSearchField(bool shown) {
	auto search = _controller->searchFieldController();
	if (search && shown) {
		auto rowView = search->createRowView(
			this,
			st::infoLayerMediaSearch);
		_searchWrap = std::move(rowView.wrap);
		_searchField = rowView.field;
		_searchField->customUpDown(true);

		const auto view = _searchWrap.get();
		widthValue(
		) | rpl::on_next([=](int newWidth) {
			view->resizeToWidth(newWidth);
			view->moveToLeft(0, 0);
		}, view->lifetime());
		view->show();
		_searchField->setFocus();
		setScrollTopSkip(view->heightNoMargins() - st::lineWidth);
	} else if (_searchWrap) {
		if (Ui::InFocusChain(this)) {
			setFocus();
		}
		_searchWrap = nullptr;
		setScrollTopSkip(0);
	}
}

int ContentWidget::scrollBottomSkip() const {
	return _scrollBottomSkip.current();
}

rpl::producer<int> ContentWidget::scrollBottomSkipValue() const {
	return _scrollBottomSkip.value();
}

rpl::producer<bool> ContentWidget::desiredBottomShadowVisibility() {
	using namespace rpl::mappers;
	return rpl::combine(
		_scroll->scrollTopValue(),
		_scrollBottomSkip.value(),
		_scroll->heightValue()
	) | rpl::map([=](int scroll, int skip, int) {
		return ((skip > 0) && (scroll < _scroll->scrollTopMax()));
	});
}

not_null<Ui::ScrollArea*> ContentWidget::scroll() const {
	return _scroll.data();
}

void ContentWidget::replaceSwipeHandler(
		Ui::Controls::SwipeHandlerArgs *incompleteArgs) {
	_swipeHandlerLifetime.destroy();
	auto args = std::move(*incompleteArgs);
	args.widget = _innerWrap;
	args.scroll = _scroll.data();
	args.onLifetime = &_swipeHandlerLifetime;
	Ui::Controls::SetupSwipeHandler(std::move(args));
}

void ContentWidget::setSwipeInterceptor(SwipeInterceptor interceptor) {
	_swipeInterceptor = std::move(interceptor);
}

void ContentWidget::setupSwipeHandler(not_null<Ui::RpWidget*> widget) {
	_swipeHandlerLifetime.destroy();

	auto update = [=](Ui::Controls::SwipeContextData data) {
		if (data.translation != 0) {
			if (!_swipeBackData.callback) {
				_swipeBackData = Ui::Controls::SetupSwipeBack(
					this,
					[]() -> std::pair<QColor, QColor> {
						return {
							st::historyForwardChooseBg->c,
							st::historyForwardChooseFg->c,
						};
					},
					data.translation < 0);
			}
			_swipeBackData.callback(data);
			return;
		} else if (_swipeBackData.lifetime) {
			_swipeBackData = {};
		}
	};

	auto init = [=](Ui::Controls::SwipeHandlerInitData data) {
		if (_swipeInterceptor) {
			auto mapped = data;
			mapped.cursorPosition = _innerWrap->entity()->mapFrom(
				_innerWrap,
				data.cursorPosition);
			auto result = _swipeInterceptor(mapped);
			if (result.callback) {
				result.callback = crl::guard(
					this,
					[this, onstack = std::move(result.callback)] {
						_swipeBackData = {};
						onstack();
					});
				return result;
			}
		}
		if (data.direction != Qt::RightToLeft) {
			return Ui::Controls::SwipeHandlerFinishData();
		}
		const auto parent = _controller->parentController();
		auto action = Fn<void()>();
		if (_controller->hasBackButton()) {
			action = [=] {
				parent->hideLayer();
				_controller->showBackFromStack();
			};
		} else if (_controller->wrap() == Wrap::Side) {
			action = [=] {
				parent->closeThirdSection();
			};
		} else if (_controller->wrap() == Wrap::Layer) {
			action = [=] {
				parent->hideLayer();
				parent->hideSpecialLayer();
			};
		}
		return action
			? Ui::Controls::DefaultSwipeBackHandlerFinishData([=] {
				checkBeforeClose(crl::guard(this, action));
			})
			: Ui::Controls::SwipeHandlerFinishData();
	};

	Ui::Controls::SetupSwipeHandler({
		.widget = widget,
		.scroll = _scroll.data(),
		.update = std::move(update),
		.init = std::move(init),
		.onLifetime = &_swipeHandlerLifetime,
	});
}

Key ContentMemento::key() const {
	if (const auto topic = this->topic()) {
		return Key(topic);
	} else if (const auto sublist = this->sublist()) {
		return Key(sublist);
	} else if (const auto savedMessages = this->savedMessages()) {
		return Key(savedMessages);
	} else if (const auto peer = this->peer()) {
		return Key(peer);
	} else if (const auto poll = this->poll()) {
		return Key(poll, pollContextId());
	} else if (const auto self = settingsSelf()) {
		return Settings::Tag{ self };
	} else if (const auto gifts = giftsPeer()) {
		return PeerGifts::Tag{
			gifts,
			giftsCollectionId(),
		};
	} else if (const auto stories = storiesPeer()) {
		return Stories::Tag{
			stories,
			storiesAlbumId(),
			storiesAddToAlbumId(),
		};
	} else if (const auto music = musicPeer()) {
		return Saved::MusicTag{ music };
	} else if (statisticsTag().peer) {
		return statisticsTag();
	} else if (const auto starref = starrefPeer()) {
		return BotStarRef::Tag(starref, starrefType());
	} else if (const auto who = reactionsWhoReadIds()) {
		return Key(who, _reactionsSelected, _pollReactionsContextId);
	} else if (const auto another = globalMediaSelf()) {
		return GlobalMedia::Tag{ another };
	} else {
		return Downloads::Tag();
	}
}

ContentMemento::ContentMemento(
	not_null<PeerData*> peer,
	Data::ForumTopic *topic,
	Data::SavedSublist *sublist,
	PeerId migratedPeerId)
: _peer(peer)
, _migratedPeerId((!topic && !sublist && peer->migrateFrom())
	? peer->migrateFrom()->id
	: 0)
, _topic(topic)
, _sublist(sublist) {
	if (_topic) {
		_peer->owner().itemIdChanged(
		) | rpl::on_next([=](const Data::Session::IdChange &change) {
			if (_topic->rootId() == change.oldId) {
				_topic = _topic->forum()->topicFor(change.newId.msg);
			}
		}, _lifetime);
	}
}

ContentMemento::ContentMemento(not_null<Data::SavedMessages*> savedMessages)
: _peer(savedMessages->session().user().get())
, _savedMessages(savedMessages) {
	Expects(!savedMessages->parentChat());
}

ContentMemento::ContentMemento(Settings::Tag settings)
: _settingsSelf(settings.self.get()) {
}

ContentMemento::ContentMemento(Downloads::Tag downloads) {
}

ContentMemento::ContentMemento(Stories::Tag stories)
: _storiesPeer(stories.peer)
, _storiesAlbumId(stories.albumId)
, _storiesAddToAlbumId(stories.addingToAlbumId) {
}

ContentMemento::ContentMemento(Saved::MusicTag music)
: _musicPeer(music.peer) {
}

ContentMemento::ContentMemento(PeerGifts::Tag gifts)
: _giftsPeer(gifts.peer)
, _giftsCollectionId(gifts.collectionId) {
}

ContentMemento::ContentMemento(Statistics::Tag statistics)
: _statisticsTag(statistics) {
}

ContentMemento::ContentMemento(BotStarRef::Tag starref)
: _starrefPeer(starref.peer)
, _starrefType(starref.type) {
}

ContentMemento::ContentMemento(GlobalMedia::Tag global)
: _globalMediaSelf(global.self) {
}

ContentMemento::ContentMemento(
	std::shared_ptr<Api::WhoReadList> whoReadIds,
	FullMsgId contextId,
	Data::ReactionId selected)
: _reactionsWhoReadIds(whoReadIds
	? whoReadIds
	: std::make_shared<Api::WhoReadList>())
, _reactionsSelected(selected)
, _pollReactionsContextId(contextId) {
}

} // namespace Info
