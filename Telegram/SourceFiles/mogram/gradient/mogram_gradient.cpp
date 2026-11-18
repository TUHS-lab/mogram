// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: change (fork): animated gradient chat background.
//
// Как это устроено и почему дёшево:
//  - кадр считается на процессоре в 128 точек по длинной стороне (около 10 тысяч
//    пикселей, доли миллисекунды) и растягивается на весь фон с интерполяцией;
//  - кадр пересчитывается только после тика таймера, а не при каждой отрисовке;
//  - таймер живёт, только пока фон реально рисуется: свернули окно, ушли в другой
//    раздел или закрыли чат, и следующий тик сам себя отменяет;
//  - энергосбережение Telegram, режим тостера, пауза и фон окна замораживают кадр,
//    но не выключают сам градиент.
#include "mogram/gradient/mogram_gradient.h"
#include "mogram/gradient/mogram_gradient_mesh.h"
#include "mogram/gradient/mogram_gradient_palette.h"

#include "mogram/appearance/mogram_appearance.h"
#include "mogram/lang/mogram_lang.h"
#include "mogram/perf/mogram_perf.h"
#include "mogram/settings/mogram_settings.h"
#include "base/timer.h"
#include "ui/effects/animation_value.h"
#include "ui/power_saving.h"
#include "ui/style/style_core.h"
#include "ui/style/style_core_palette.h"
#include "styles/palette.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

#include <rpl/rpl.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>

namespace Mogram::Gradient {
namespace {

namespace S = ::Mogram::Settings;

constexpr auto kEnabled = "look.gradient.enabled";
constexpr auto kPreset = "look.gradient.preset";
constexpr auto kSpeed = "look.gradient.speed";
constexpr auto kFps = "look.gradient.fps";
constexpr auto kDim = "look.gradient.dim";
constexpr auto kCustom = "look.gradient.color";
constexpr auto kCustomPreset = int(Preset::Custom);

constexpr auto kMaxStep = crl::time(100);

// Названия идут в порядке enum Preset.
[[nodiscard]] const char *PresetName(int index) {
	switch (Preset(index)) {
	case Preset::Theme: return "Из темы";
	case Preset::Aurora: return "Аврора";
	case Preset::Sunset: return "Закат";
	case Preset::Ocean: return "Океан";
	case Preset::Graphite: return "Графит";
	case Preset::Neon: return "Неон";
	case Preset::Forest: return "Лес";
	case Preset::Custom: break;
	}
	return "Свои цвета";
}

struct State {
	bool enabled = false;
	bool subscribed = false;
	bool dirty = true;
	bool paintedSinceTick = false;
	bool paused = false; // последний кадр нарисован на паузе (открыто окно поверх)
	double phase = 0.;
	double speed = 0.15; // радиан в секунду
	int fps = 24;
	crl::time lastTick = 0;
	Palette palette;
	QSize imageFor;
	QImage image;
	QPointer<QWidget> widget;
	std::vector<QPointer<QWidget>> extra;
	std::optional<base::Timer> timer;
	rpl::lifetime lifetime;
};

[[nodiscard]] State &Data() {
	static auto result = State();
	return result;
}

// Тёмная ли тема: по яркости подложки окна, а не по названию темы.
[[nodiscard]] bool ThemeIsDark() {
	return QColor(st::windowBg->c).lightnessF() < 0.5;
}

[[nodiscard]] Palette AccentPalette(bool dark) {
	const auto accent = QColor(st::windowBgActive->c);
	// Нейтральный (чёрно-белый) акцент: спокойный тёмно-серый градиент.
	const auto lab = LabFromSrgb(accent.red(), accent.green(), accent.blue());
	if (std::hypot(double(lab.a), double(lab.b)) < 0.05) {
		return PresetPalette(Preset::Graphite, dark);
	}
	return ThemePalette(
		HueOf(accent.red(), accent.green(), accent.blue()),
		dark);
}

void ReadPalette(State &st) {
	const auto index = std::clamp(
		S::Int(QString::fromUtf8(kPreset)),
		0,
		kCustomPreset);
	const auto dark = ThemeIsDark();
	const auto muted = std::clamp(S::Int(QString::fromUtf8(kDim)), 0, 80) / 100.f;
	if (Preset(index) == Preset::Custom) {
		// Свои цвета берутся как есть, пустые или неверные заменяет палитра темы.
		const auto fallback = AccentPalette(dark);
		for (auto i = 0; i != 4; ++i) {
			const auto color = Appearance::ColorSetting(
				QString::fromUtf8(kCustom) + QString::number(i + 1));
			st.palette[i] = color.isValid()
				? LabFromSrgb(color.red(), color.green(), color.blue())
				: fallback[i];
		}
	} else if (Preset(index) == Preset::Theme) {
		// Акцент темы задаёт оттенок: сменили тему, и фон перекрасился.
		st.palette = AccentPalette(dark);
	} else {
		st.palette = PresetPalette(Preset(index), dark);
	}
	st.palette = Muted(st.palette, dark, muted);
}

[[nodiscard]] const QPixmap &NoiseTile() {
	static const auto tile = [] {
		constexpr auto kSide = 96;
		auto image = QImage(kSide, kSide, QImage::Format_ARGB32_Premultiplied);
		auto seed = quint32(2463534242u);
		for (auto y = 0; y != kSide; ++y) {
			const auto line = reinterpret_cast<QRgb*>(image.scanLine(y));
			for (auto x = 0; x != kSide; ++x) {
				seed ^= seed << 13;
				seed ^= seed >> 17;
				seed ^= seed << 5;
				// Прибавка 0..3 уровня к каждому каналу: полупрозрачный белый
				// и чёрный шум на тёмном фоне почти ничего не меняли, и линии
				// уровней были видны. Сложение разбивает их по-настоящему.
				const auto level = int(seed % 4);
				line[x] = qRgba(level, level, level, 255);
			}
		}
		return QPixmap::fromImage(std::move(image));
	}();
	return tile;
}

void Apply() {
	auto &st = Data();
	st.enabled = S::Bool(QString::fromUtf8(kEnabled));
	// 1..100 в ползунке: 30 это около сорока секунд на круг.
	st.speed = 0.005 * std::clamp(S::Int(QString::fromUtf8(kSpeed)), 1, 100);
	const auto fps = S::Int(QString::fromUtf8(kFps));
	st.fps = (fps == 0) ? 15 : (fps == 2) ? 30 : 24;
	ReadPalette(st);
	st.dirty = true;
	if (st.timer && st.timer->isActive()) {
		st.timer->callEach(1000 / st.fps);
	}
	if (const auto widget = st.widget.data()) {
		widget->update();
	}
	for (const auto &extra : st.extra) {
		if (const auto widget = extra.data()) {
			widget->update();
		}
	}
}

[[nodiscard]] bool CanAnimate() {
	return !PowerSaving::On(PowerSaving::kChatBackground)
		&& !anim::Disabled()
		&& (QGuiApplication::applicationState() == Qt::ApplicationActive)
		&& Perf::UserIsLooking()
		&& !S::Bool(u"lag.toaster"_q);
}

void Tick() {
	auto &st = Data();
	const auto now = crl::now();
	const auto step = std::min(now - st.lastTick, kMaxStep);
	st.lastTick = now;
	if (!st.enabled || !st.paintedSinceTick || st.paused || !CanAnimate()) {
		// Никто не смотрит на фон: таймер больше не нужен до следующей отрисовки.
		st.paintedSinceTick = false;
		st.timer->cancel();
		return;
	}
	st.paintedSinceTick = false;
	st.phase += step * 0.001 * st.speed;
	st.dirty = true;
	if (const auto widget = st.widget.data()) {
		widget->update();
	}
	for (const auto &extra : st.extra) {
		if (const auto widget = extra.data()) {
			widget->update();
		}
	}
}

void EnsureTimer() {
	auto &st = Data();
	if (!st.timer) {
		st.timer.emplace([] { Tick(); });
		// Таймер статический: убираем его, пока Qt ещё жив.
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
			Data().timer.reset();
		});
	}
	if (!st.timer->isActive()) {
		st.lastTick = crl::now();
		st.timer->callEach(1000 / st.fps);
	}
}

void Subscribe() {
	auto &st = Data();
	if (st.subscribed) {
		return;
	}
	st.subscribed = true;
	S::Changed() | rpl::filter([](const QString &key) {
		return key.startsWith(u"look.gradient."_q);
	}) | rpl::on_next([] {
		Apply();
	}, st.lifetime);
	// Смена темы или акцента: палитра «из темы» пересчитывается сразу.
	style::PaletteChanged() | rpl::on_next([] {
		Apply();
	}, st.lifetime);
	Apply();
}

} // namespace

void RegisterSettings() {
	const auto U = [](const char *s) { return QString::fromUtf8(s); };
	const auto section = U(Mogram::Lang::Tr("Градиентный фон"));
	const auto add = [&](S::Entry entry) {
		entry.section = section;
		entry.category = u"look"_q;
		entry.sectionIcon = u"palette"_q;
		S::Register(std::move(entry));
	};
	add({
		.key = U(kEnabled),
		.title = U(Mogram::Lang::Tr("Анимированный градиент")),
		.description = U(Mogram::Lang::Tr("Плавно переливающийся фон чата в цветах вашей темы. Замирает при энергосбережении и когда окно не в фокусе.")),
		.kind = S::Kind::Bool,
		.defaultValue = true,
		.sectionNote = U(Mogram::Lang::Tr("Кадр считается в маленьком размере и растягивается, поэтому нагрузка почти нулевая. Когда фон не виден, анимация не работает вовсе.")),
	});
	auto presets = QStringList();
	for (auto i = 0; i <= kCustomPreset; ++i) {
		presets.push_back(U(Mogram::Lang::Tr(PresetName(i))));
	}
	add({
		.key = U(kPreset),
		.title = U(Mogram::Lang::Tr("Палитра")),
		.kind = S::Kind::Choice,
		// По умолчанию спокойный графит.
		.defaultValue = int(Preset::Graphite),
		.choices = presets,
		.dependsOn = U(kEnabled),
	});
	add({
		.key = U(kSpeed),
		.title = U(Mogram::Lang::Tr("Скорость")),
		.kind = S::Kind::Int,
		.defaultValue = 45,
		.minValue = 1,
		.maxValue = 100,
		.dependsOn = U(kEnabled),
	});
	add({
		.key = U(kDim),
		.title = U(Mogram::Lang::Tr("Приглушение, %")),
		.description = U(Mogram::Lang::Tr("Чем сильнее, тем спокойнее фон и лучше читается текст.")),
		.kind = S::Kind::Int,
		.defaultValue = 0,
		.minValue = 0,
		.maxValue = 80,
		.dependsOn = U(kEnabled),
	});
	add({
		.key = U(kFps),
		.title = U(Mogram::Lang::Tr("Частота кадров")),
		.description = U(Mogram::Lang::Tr("Меньше кадров значит меньше нагрузки. Движение медленное, 15 почти незаметно.")),
		.kind = S::Kind::Choice,
		.defaultValue = 0,
		.choices = { U("15"), U("24"), U("30") },
		.dependsOn = U(kEnabled),
	});
	for (auto i = 1; i <= 4; ++i) {
		add({
			.key = U(kCustom) + QString::number(i),
			.title = U(Mogram::Lang::Tr("Цвет ")) + QString::number(i),
			.description = (i == 1)
				? U(Mogram::Lang::Tr("Нажмите на строку, чтобы выбрать цвет."))
				: QString(),
			.kind = S::Kind::Color,
			.defaultValue = QString(),
			// Строки цветов видны, только когда выбрана палитра «Свои цвета».
			.dependsOn = U(kPreset) + u"=="_q + QString::number(kCustomPreset),
		});
	}
}

void AttachExtra(not_null<QWidget*> widget) {
	auto &extra = Data().extra;
	extra.erase(
		std::remove_if(extra.begin(), extra.end(), [](const auto &w) {
			return w.isNull();
		}),
		extra.end());
	extra.push_back(widget.get());
}

void Attach(not_null<QWidget*> widget) {
	auto &st = Data();
	st.widget = widget.get();
	Subscribe();
}

bool Paint(QPainter &p, QSize fill, QRect clip, bool paused) {
	auto &st = Data();
	if (!st.enabled || fill.isEmpty()) {
		return false;
	}
	const auto size = MeshSizeFor(fill.width(), fill.height());
	if (size.width <= 0) {
		return false;
	}
	if (st.dirty
		|| st.image.isNull()
		|| st.imageFor != QSize(size.width, size.height)) {
		if (st.image.size() != QSize(size.width, size.height)) {
			st.image = QImage(
				size.width,
				size.height,
				QImage::Format_RGB32);
		}
		st.imageFor = QSize(size.width, size.height);
		RenderMesh(
			reinterpret_cast<std::uint32_t*>(st.image.bits()),
			size.width,
			size.height,
			st.palette,
			st.phase);
		st.dirty = false;
	}
	p.save();
	p.setClipRect(clip, Qt::IntersectClip);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.drawImage(QRect(QPoint(), fill), st.image);
	// Тонкий шум ломает полосы квантования 8 бит на плавном градиенте.
	p.setCompositionMode(QPainter::CompositionMode_Plus);
	p.drawTiledPixmap(QRect(QPoint(), fill), NoiseTile());
	p.restore();

	st.paintedSinceTick = true;
	st.paused = paused;
	if (!paused && CanAnimate()) {
		EnsureTimer();
	}
	return true;
}

} // namespace Mogram::Gradient
