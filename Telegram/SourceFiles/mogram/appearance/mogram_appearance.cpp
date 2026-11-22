// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 1a4e9630
// Mogram: оформление, настраиваемое в «Кастомизации».
#include "mogram/appearance/mogram_appearance.h"
#include "mogram/lang/mogram_lang.h"

#include "mogram/settings/mogram_settings.h"
#include "mogram/music/mogram_motion.h"
#include "ui/style/style_core.h"
#include "ui/style/style_core_palette.h"
#include "styles/palette.h"

#include <array>
#include <vector>
#include <algorithm>
#include <QtCore/QHash>
#include <rpl/rpl.h>


namespace Mogram::Appearance {
namespace {

namespace S = Settings;

constexpr auto kBubblesOn = "bubbles.enabled";
constexpr auto kOutColor = "bubbles.out_color";
constexpr auto kInColor = "bubbles.in_color";
constexpr auto kOutText = "bubbles.out_text";
constexpr auto kInText = "bubbles.in_text";

rpl::lifetime Lifetime;
bool Applying = false;

struct Slot {
	const char *name;
	style::color color;
	QColor original;
	QColor lastSet;
};

std::vector<Slot> &Slots() {
	static auto result = std::vector<Slot>{
		{ "msgOutBg", st::msgOutBg },
		{ "msgOutBgSelected", st::msgOutBgSelected },
		{ "msgOutShadow", st::msgOutShadow },
		{ "msgOutShadowSelected", st::msgOutShadowSelected },
		{ "msgInBg", st::msgInBg },
		{ "msgInBgSelected", st::msgInBgSelected },
		{ "msgInShadow", st::msgInShadow },
		{ "msgInShadowSelected", st::msgInShadowSelected },
		{ "historyTextOutFg", st::historyTextOutFg },
		{ "historyTextInFg", st::historyTextInFg },
	};
	return result;
}

[[nodiscard]] QColor Shifted(const QColor &color, int percent) {
	return color.lighter(100 + percent);
}

[[nodiscard]] QColor Shadow(const QColor &color) {
	auto result = color.darker(150);
	result.setAlpha(60);
	return result;
}

// Mogram: нейтральные (серые) акценты в чатах. Синий остаётся в настройках и
// важных экранах, а выбранный чат, значок «Избранного», боковая панель папок и
// свои сообщения по умолчанию серые. Работает на тёмных темах.
constexpr auto kNeutralKey = "look.neutral_accents";

[[nodiscard]] bool NeutralNow() {
	return S::Bool(QString::fromUtf8(kNeutralKey))
		&& QColor(st::windowBg->c).lightnessF() < 0.5;
}

struct AccentSlot {
	const char *name;
	style::color color;
	QColor gray;
	QColor original;
	QColor lastSet;
	bool follow = false; // цвет окна берётся из windowBg: фон окон единый
};

std::vector<AccentSlot> &AccentSlots() {
	static auto result = std::vector<AccentSlot>{
		{ "boxBg", st::boxBg, QColor(), QColor(), QColor(), true },
		{ "dialogsBgActive", st::dialogsBgActive, QColor(0x3a, 0x3a, 0x3e) },
		{ "historyPeerSavedMessagesBg", st::historyPeerSavedMessagesBg, QColor(0x63, 0x63, 0x66) },
		{ "historyPeerSavedMessagesBg2", st::historyPeerSavedMessagesBg2, QColor(0x48, 0x48, 0x4a) },
		{ "sideBarBgActive", st::sideBarBgActive, QColor(0x3a, 0x3a, 0x3e) },
		{ "sideBarTextFgActive", st::sideBarTextFgActive, QColor(0xf2, 0xf2, 0xf7) },
		{ "sideBarIconFgActive", st::sideBarIconFgActive, QColor(0xf2, 0xf2, 0xf7) },
		{ "sideBarBadgeBg", st::sideBarBadgeBg, QColor(0x63, 0x63, 0x66) },
		// Синий текст статусов и заголовков (в сети, закреплённое) серый.
		{ "windowActiveTextFg", st::windowActiveTextFg, QColor(0xb4, 0xb4, 0xbb) },
		{ "dialogsUnreadBg", st::dialogsUnreadBg, QColor(0x6a, 0x6a, 0x70) },
		{ "dialogsUnreadBgMuted", st::dialogsUnreadBgMuted, QColor(0x4e, 0x4e, 0x55) },
		{ "dialogsUnreadBgMutedOver", st::dialogsUnreadBgMutedOver, QColor(0x58, 0x58, 0x60) },
		// Активные кнопки и ползунки тёмно-серые, а не светлые.
		{ "windowBgActive", st::windowBgActive, QColor(0x5e, 0x5e, 0x66) },
		{ "activeButtonBg", st::activeButtonBg, QColor(0x5e, 0x5e, 0x66) },
		{ "activeButtonBgOver", st::activeButtonBgOver, QColor(0x6a, 0x6a, 0x72) },
		{ "activeButtonBgRipple", st::activeButtonBgRipple, QColor(0x4f, 0x4f, 0x56) },
		{ "sliderBgActive", st::sliderBgActive, QColor(0x74, 0x74, 0x7c) },
		{ "historySendIconFg", st::historySendIconFg, QColor(0x9e, 0x9e, 0xa6) },
		{ "historySendIconFgOver", st::historySendIconFgOver, QColor(0xb8, 0xb8, 0xc0) },
		{ "dialogsTextFgService", st::dialogsTextFgService, QColor(0xb4, 0xb4, 0xbb) },
		{ "dialogsTextFgServiceOver", st::dialogsTextFgServiceOver, QColor(0xb4, 0xb4, 0xbb) },
		{ "historyLinkInFg", st::historyLinkInFg, QColor(0xb4, 0xb4, 0xbb) },
		{ "historyLinkInFgSelected", st::historyLinkInFgSelected, QColor(0xc8, 0xc8, 0xcf) },
		{ "historyLinkOutFg", st::historyLinkOutFg, QColor(0xb4, 0xb4, 0xbb) },
		{ "historyLinkOutFgSelected", st::historyLinkOutFgSelected, QColor(0xc8, 0xc8, 0xcf) },
		{ "msgInReplyBarColor", st::msgInReplyBarColor, QColor(0x8e, 0x8e, 0x95) },
		{ "msgWaveformInActive", st::msgWaveformInActive, QColor(0x9a, 0x9a, 0xa2) },
		// Значки непрочитанного: тёмно-серые кружки с белой цифрой, видно и на
		// выбранном чате, и на выбранной папке.
		{ "sideBarBadgeBgActive", st::sideBarBadgeBgActive, QColor(0x84, 0x84, 0x8c) },
		{ "sideBarBadgeBgMuted", st::sideBarBadgeBgMuted, QColor(0x54, 0x54, 0x5a) },
		{ "sideBarBadgeBgMutedActive", st::sideBarBadgeBgMutedActive, QColor(0x70, 0x70, 0x78) },
		{ "dialogsUnreadBgActive", st::dialogsUnreadBgActive, QColor(0x84, 0x84, 0x8c) },
		{ "dialogsUnreadBgMutedActive", st::dialogsUnreadBgMutedActive, QColor(0x70, 0x70, 0x78) },
		{ "dialogsUnreadFgActive", st::dialogsUnreadFgActive, QColor(0xff, 0xff, 0xff) },
		// Галочка проверки и подобные значки: средне-серые, не белые.
		{ "dialogsVerifiedIconBg", st::dialogsVerifiedIconBg, QColor(0x8e, 0x8e, 0x95) },
		{ "dialogsVerifiedIconBgOver", st::dialogsVerifiedIconBgOver, QColor(0x9a, 0x9a, 0xa1) },
		{ "dialogsVerifiedIconBgActive", st::dialogsVerifiedIconBgActive, QColor(0x9a, 0x9a, 0xa1) },
		{ "dialogsVerifiedIconFg", st::dialogsVerifiedIconFg, QColor(0x1c, 0x1c, 0x1e) },
		{ "dialogsVerifiedIconFgOver", st::dialogsVerifiedIconFgOver, QColor(0x1c, 0x1c, 0x1e) },
		{ "dialogsVerifiedIconFgActive", st::dialogsVerifiedIconFgActive, QColor(0x1c, 0x1c, 0x1e) },
		// Круги воспроизведения аудио: тёмно-серые, стрелка светлая.
		{ "msgFileInBg", st::msgFileInBg, QColor(0x5e, 0x5e, 0x66) },
		{ "msgFileInBgOver", st::msgFileInBgOver, QColor(0x6a, 0x6a, 0x72) },
		{ "msgFileInBgSelected", st::msgFileInBgSelected, QColor(0x6a, 0x6a, 0x72) },
		{ "historyFileInIconFg", st::historyFileInIconFg, QColor(0xee, 0xee, 0xf2) },
		{ "historyFileInIconFgSelected", st::historyFileInIconFgSelected, QColor(0xee, 0xee, 0xf2) },
		{ "historyFileInRadialFg", st::historyFileInRadialFg, QColor(0xee, 0xee, 0xf2) },
		{ "historyFileInRadialFgSelected", st::historyFileInRadialFgSelected, QColor(0xee, 0xee, 0xf2) },
		// Кнопки звонка: тёмно-серые полупрозрачные, а не почти белые.
		{ "callIconBgActive", st::callIconBgActive, QColor(255, 255, 255, 0x42) },
		{ "callIconFgActive", st::callIconFgActive, QColor(0xff, 0xff, 0xff) },
		// Линии: тонкие светло-серые, а не чёрные.
		{ "menuSeparatorFg", st::menuSeparatorFg, QColor(255, 255, 255, 30) },
	};
	return result;
}

// Mogram: серая тема целиком. Все цвета палитры с холодным оттенком (голубой,
// синий, фиолетовый) обесцвечиваются до нейтрального серого, красные, зелёные
// и тёплые остаются. Так нет «забытых» синих мест, как при правке по одному.
struct GenericColor {
	QColor original;
	QColor lastSet;
};

[[nodiscard]] QHash<QString, GenericColor> &GenericColors() {
	static auto result = QHash<QString, GenericColor>();
	return result;
}

[[nodiscard]] QColor Neutralized(const QColor &color) {
	if (color.alpha() == 0) {
		return color;
	}
	const auto hsl = color.toHsl();
	const auto hue = hsl.hslHueF() * 360.;
	const auto saturation = hsl.hslSaturationF();
	if (hue < 0. || saturation <= 0.10 || hue < 170. || hue > 305.) {
		return color;
	}
	auto result = QColor::fromHslF(
		float(hsl.hslHueF()),
		std::min(float(saturation), 0.05f),
		float(hsl.lightnessF()));
	result.setAlpha(color.alpha());
	return result;
}

void ApplyGenericGray(
		const std::vector<AccentSlot> &accents,
		const std::vector<Slot> &bubbles) {
	const auto on = S::Bool(QString::fromUtf8(kNeutralKey));
	const auto skip = [&](QLatin1String name) {
		// Premium, Звёзды, подарки и цветные значки настроек сохраняют свои цвета.
		const auto text = QString::fromLatin1(name.data(), name.size());
		if (text.contains(u"remium"_q, Qt::CaseInsensitive)
			|| text.contains(u"settingsIcon"_q)
			|| text.contains(u"credits"_q, Qt::CaseInsensitive)
			|| text.contains(u"stars"_q, Qt::CaseInsensitive)
			|| text.contains(u"gift"_q, Qt::CaseInsensitive)
			|| text.contains(u"boost"_q, Qt::CaseInsensitive)) {
			return true;
		}
		for (const auto &slot : accents) {
			if (QLatin1String(slot.name) == name) {
				return true;
			}
		}
		for (const auto &slot : bubbles) {
			if (QLatin1String(slot.name) == name) {
				return true;
			}
		}
		return false;
	};
	auto changed = false;
	const auto palette = style::main_palette::get();
	auto &stored = GenericColors();
	for (const auto &row : style::main_palette::data()) {
		if (skip(row.name)) {
			continue;
		}
		const auto index = style::internal::GetPaletteIndex(row.name);
		if (index < 0) {
			continue;
		}
		const auto now = QColor(palette->colorAtIndex(index)->c);
		auto &entry = stored[QString::fromLatin1(row.name.data(), row.name.size())];
		// Цвет не наш: тема его перезаписала, это новый исходный цвет.
		if (!entry.lastSet.isValid() || now != entry.lastSet) {
			entry.original = now;
			entry.lastSet = QColor();
		}
		const auto target = on ? Neutralized(entry.original) : entry.original;
		if (target != now) {
			style::main_palette::setColor(
				row.name,
				uchar(target.red()),
				uchar(target.green()),
				uchar(target.blue()),
				uchar(target.alpha()));
			changed = true;
		}
		entry.lastSet = target;
	}
	if (changed) {
		style::NotifyPaletteChanged();
	}
}

void ApplyAccents() {
	if (Applying) {
		return;
	}
	Applying = true;
	ApplyGenericGray(AccentSlots(), Slots());
	const auto neutral = NeutralNow();
	auto changed = false;
	for (auto &slot : AccentSlots()) {
		const auto now = QColor(slot.color->c);
		// Цвет не наш: тема его перезаписала, это новый исходный цвет.
		if (!slot.lastSet.isValid() || now != slot.lastSet) {
			slot.original = now;
			slot.lastSet = QColor();
		}
		const auto gray = slot.follow ? QColor(st::windowBg->c) : slot.gray;
		const auto target = neutral ? gray : slot.original;
		if (target != now) {
			style::main_palette::setColor(
				QLatin1String(slot.name),
				uchar(target.red()),
				uchar(target.green()),
				uchar(target.blue()),
				uchar(target.alpha()));
			changed = true;
		}
		slot.lastSet = target;
	}
	if (changed) {
		style::NotifyPaletteChanged();
	}
	Applying = false;
}

void Apply() {
	if (Applying) {
		return;
	}
	Applying = true;
	const auto on = S::Bool(kBubblesOn);
	// Серая тема: сообщения тёмные, без тени под пузырём.
	const auto neutral = !on && NeutralNow();
	const auto neutralOut = neutral ? QColor(0x2e, 0x2e, 0x33) : QColor();
	const auto neutralIn = neutral ? QColor(0x24, 0x24, 0x27) : QColor();
	const auto noShadow = QColor(0, 0, 0, 0);
	const auto out = on ? ColorSetting(kOutColor) : neutralOut;
	const auto in = on ? ColorSetting(kInColor) : neutralIn;
	const auto outText = on ? ColorSetting(kOutText) : QColor();
	const auto inText = on ? ColorSetting(kInText) : QColor();
	const auto wanted = std::vector<QColor>{
		out.isValid() ? out : QColor(),
		out.isValid() ? Shifted(out, 12) : QColor(),
		neutral ? noShadow : (out.isValid() ? Shadow(out) : QColor()),
		neutral ? noShadow : (out.isValid() ? Shadow(Shifted(out, 12)) : QColor()),
		in.isValid() ? in : QColor(),
		in.isValid() ? Shifted(in, 12) : QColor(),
		neutral ? noShadow : (in.isValid() ? Shadow(in) : QColor()),
		neutral ? noShadow : (in.isValid() ? Shadow(Shifted(in, 12)) : QColor()),
		outText,
		inText,
	};
	auto &slots = Slots();
	auto changed = false;
	for (auto i = 0; i != int(slots.size()); ++i) {
		auto &slot = slots[i];
		const auto now = QColor(slot.color->c);
		// Цвет не наш: тема его перезаписала, это новый исходный цвет.
		if (!slot.lastSet.isValid() || now != slot.lastSet) {
			slot.original = now;
			slot.lastSet = QColor();
		}
		const auto target = wanted[i].isValid() ? wanted[i] : slot.original;
		if (target != now) {
			style::main_palette::setColor(
				QLatin1String(slot.name),
				uchar(target.red()),
				uchar(target.green()),
				uchar(target.blue()),
				uchar(target.alpha()));
			changed = true;
		}
		slot.lastSet = target;
	}
	if (changed) {
		style::NotifyPaletteChanged();
	}
	Applying = false;
}

// Кнопки свернуть, развернуть и закрыть окно без своей подложки: их фон
// совпадает с полосой заголовка при любой теме, а не только со встроенной.
void ApplyTitleButtons() {
	if (Applying) {
		return;
	}
	Applying = true;
	const auto target = QColor(st::titleBgActive->c);
	auto changed = false;
	struct Item {
		const char *name;
		style::color color;
	};
	const auto items = std::array<Item, 4>{ {
		{ "titleButtonBg", st::titleButtonBg },
		{ "titleButtonBgActive", st::titleButtonBgActive },
		{ "titleButtonCloseBg", st::titleButtonCloseBg },
		{ "titleButtonCloseBgActive", st::titleButtonCloseBgActive },
	} };
	for (const auto &item : items) {
		if (QColor(item.color->c) != target) {
			style::main_palette::setColor(
				QLatin1String(item.name),
				uchar(target.red()),
				uchar(target.green()),
				uchar(target.blue()),
				uchar(target.alpha()));
			changed = true;
		}
	}
	if (changed) {
		style::NotifyPaletteChanged();
	}
	Applying = false;
}

} // namespace

QColor ColorSetting(const QString &key) {
	const auto text = S::String(key).trimmed();
	if (text.isEmpty()) {
		return QColor();
	}
	return QColor(text.startsWith('#') ? text : (u"#"_q + text));
}

int SelectionLimit() {
	return S::Bool(u"chats.select_3000"_q) ? 3000 : 100;
}

void RegisterSettings() {
	const auto section = QString::fromUtf8(Mogram::Lang::Tr("Пузыри сообщений"));
	const auto add = [&](S::Entry entry) {
		entry.section = section;
		entry.category = u"look"_q;
		entry.sectionIcon = u"hide"_q;
		S::Register(std::move(entry));
	};
	add({
		.key = u"bubbles.enabled"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Свои цвета пузырьков")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Выключено: пузырьки рисуются цветами темы.")),
		.kind = S::Kind::Bool,
		.defaultValue = false,
		.sectionNote = QString::fromUtf8(
			Mogram::Lang::Tr("Цвет пишется как #RRGGBB. Пустое поле оставляет цвет темы.")),
	});
	add({
		.key = QString::fromUtf8(kNeutralKey),
		.title = QString::fromUtf8(Mogram::Lang::Tr("Серые акценты в чатах")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Выбранный чат, Избранное, панель папок и свои сообщения серые, синий остаётся в настройках.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	add({
		.key = u"look.bubble_square"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Менее скруглённые сообщения")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Пузыри с меньшим радиусом углов.")),
		.kind = S::Kind::Bool,
		.defaultValue = false,
		.requiresRestart = true,
	});
	const auto color = [&](const char *key, const char *title) {
		add({
			.key = QString::fromUtf8(key),
			.title = QString::fromUtf8(title),
			.kind = S::Kind::Color,
			.defaultValue = QString(),
			.dependsOn = u"bubbles.enabled"_q,
		});
	};
	color(kOutColor, Mogram::Lang::Tr("Мои сообщения: фон"));
	color(kOutText, Mogram::Lang::Tr("Мои сообщения: текст"));
	color(kInColor, Mogram::Lang::Tr("Чужие сообщения: фон"));
	color(kInText, Mogram::Lang::Tr("Чужие сообщения: текст"));

	{
		const auto chats = QString::fromUtf8(Mogram::Lang::Tr("Сообщения"));
		const auto chatEntry = [&](S::Entry entry) {
			entry.section = chats;
			entry.category = u"chats"_q;
			entry.sectionIcon = u"chats"_q;
			S::Register(std::move(entry));
		};
		chatEntry({
			.key = u"chats.select_3000"_q,
			.title = QString::fromUtf8(Mogram::Lang::Tr("Выделять до 3000 сообщений")),
			.description = QString::fromUtf8(Mogram::Lang::Tr("Вместо ста. Удаление и пересылка сами делятся на части.")),
			.kind = S::Kind::Bool,
			.defaultValue = true,
		});
		chatEntry({
			.key = u"chats.edited_pencil"_q,
			.title = QString::fromUtf8(Mogram::Lang::Tr("Короткая надпись «изм.»")),
			.description = QString::fromUtf8(Mogram::Lang::Tr("У изменённого сообщения возле времени стоит «изм.» вместо «изменено».")),
			.kind = S::Kind::Bool,
			.defaultValue = true,
		});
	}
	const auto player = QString::fromUtf8(Mogram::Lang::Tr("Плеер"));
	const auto playerEntry = [&](S::Entry entry) {
		entry.section = player;
		entry.category = u"look"_q;
		entry.sectionIcon = u"hide"_q;
		S::Register(std::move(entry));
	};
	playerEntry({
		.key = u"player.cover_colors"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Цвета плеера из обложки")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Фон полноэкранного плеера под цвет обложки и лёгкий оттенок на панели воспроизведения.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	playerEntry({
		.key = u"player.vinyl"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Обложка в виде диска")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("В верхней панели плеера обложка круглая, как виниловый диск: крутится при воспроизведении и плавно останавливается на паузе.")),
		.kind = S::Kind::Bool,
		.defaultValue = false,
	});
	playerEntry({
		.key = u"player.autoplay_next"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Автовоспроизведение следующего")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("После трека, голосового или кружка сам включается следующий.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	playerEntry({
		.key = u"music.smart_stream"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Умный поток музыки")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Подборка «Для вас» играет без конца: очередь сама пополняется под ваш вкус и текущее настроение, без повторов и пропущенного.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
	});
	playerEntry({
		.key = u"music.hint_sticker"_q,
		.title = u"music.hint_sticker"_q,
		.kind = S::Kind::String,
		.defaultValue = QString(),
		.hidden = true,
	});
	playerEntry({
		.key = u"lyrics.enabled"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Текст песни")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Панель со словами справа от чата, текущая строка подсвечивается. Включается кнопкой в панели плеера.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
		.hidden = true,
	});
	playerEntry({
		.key = u"lyrics.plasma"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Оттенок обложки за текстом")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Лёгкий оттенок обложки на фоне текста песни.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
		.dependsOn = u"lyrics.enabled"_q,
	});
	playerEntry({
		.key = u"lyrics.comments"_q,
		.title = QString::fromUtf8(Mogram::Lang::Tr("Комментарии SoundCloud")),
		.description = QString::fromUtf8(Mogram::Lang::Tr("Аватарки справа от текущей строки, а без текста песни на полосе прогресса. Наведи, чтобы прочитать; колёсико переключает комментарии в группе.")),
		.kind = S::Kind::Bool,
		.defaultValue = false,
	});
	Music::RegisterMotionSettings();
}

void Start() {
	S::Changed(
	) | rpl::filter([](const QString &key) {
		return key.startsWith(u"bubbles."_q)
			|| key == QString::fromUtf8(kNeutralKey);
	}) | rpl::on_next([] {
		Apply();
		ApplyAccents();
	}, Lifetime);
	style::PaletteChanged(
	) | rpl::on_next([] {
		Apply();
		ApplyAccents();
		ApplyTitleButtons();
	}, Lifetime);
	Apply();
	ApplyAccents();
	ApplyTitleButtons();
}

} // namespace Mogram::Appearance
