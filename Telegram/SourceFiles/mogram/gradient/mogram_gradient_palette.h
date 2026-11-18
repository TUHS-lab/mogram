// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: change (fork): animated gradient background, palettes.
// Цвета считаются в OKLab/OKLCH: оттенки одной яркости не «грязнятся» при
// смешивании, а яркость и насыщенность задаются отдельно для тёмной и светлой
// темы. Ничего из Qt, чтобы можно было проверять без сборки клиента.
#pragma once

#include <array>
#include <cmath>

namespace Mogram::Gradient {

struct Lab {
	float L = 0.f;
	float a = 0.f;
	float b = 0.f;
};

using Palette = std::array<Lab, 4>;

namespace details {

inline constexpr double kPi = 3.14159265358979323846;

inline void LabToLinear(const Lab &c, float (&out)[3]) {
	const auto l = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
	const auto m = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
	const auto s = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;
	const auto l3 = l * l * l, m3 = m * m * m, s3 = s * s * s;
	out[0] = +4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
	out[1] = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
	out[2] = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;
}

[[nodiscard]] inline bool InGamut(const Lab &c) {
	float rgb[3];
	LabToLinear(c, rgb);
	for (const auto v : rgb) {
		if (v < -0.001f || v > 1.001f) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] inline Lab FromLch(double L, double C, double hueDegrees) {
	const auto h = hueDegrees * kPi / 180.;
	return {
		float(L),
		float(C * std::cos(h)),
		float(C * std::sin(h)),
	};
}

// Если цвет не влезает в sRGB, убавляем насыщенность, а не обрезаем каналы:
// обрезка даёт грязные рёбра там, где один канал упёрся в предел.
[[nodiscard]] inline Lab FitToGamut(double L, double C, double hue) {
	auto result = FromLch(L, C, hue);
	if (InGamut(result)) {
		return result;
	}
	auto lo = 0., hi = C;
	for (auto i = 0; i != 18; ++i) {
		const auto mid = (lo + hi) / 2;
		(InGamut(FromLch(L, mid, hue)) ? lo : hi) = mid;
	}
	return FromLch(L, lo, hue);
}

[[nodiscard]] inline double SrgbToLinear(int value) {
	const auto c = value / 255.;
	return (c <= 0.04045) ? (c / 12.92) : std::pow((c + 0.055) / 1.055, 2.4);
}

struct Profile {
	double lightness[4];
	double chroma[4];
};

// Mogram: темнее и спокойнее, чтобы фон не слепил и текст читался.
inline constexpr Profile kDark = {
	{ 0.50, 0.41, 0.33, 0.24 },
	{ 0.15, 0.13, 0.11, 0.06 },
};
inline constexpr Profile kDarkVivid = {
	{ 0.50, 0.58, 0.45, 0.38 },
	{ 0.20, 0.24, 0.20, 0.14 },
};
inline constexpr Profile kLight = {
	{ 0.93, 0.90, 0.95, 0.88 },
	{ 0.07, 0.075, 0.05, 0.06 },
};

[[nodiscard]] inline Palette Build(
		const Profile &profile,
		const double (&hues)[4],
		double chromaScale = 1.) {
	auto result = Palette();
	for (auto i = 0; i != 4; ++i) {
		result[i] = FitToGamut(
			profile.lightness[i],
			profile.chroma[i] * chromaScale,
			hues[i]);
	}
	return result;
}

} // namespace details

enum class Preset {
	Theme,
	Aurora,
	Sunset,
	Ocean,
	Graphite,
	Neon,
	Forest,
	Custom,
};

inline constexpr auto kPresetCount = 8;

[[nodiscard]] inline Lab LabFromSrgb(int r, int g, int b) {
	const auto lr = details::SrgbToLinear(r);
	const auto lg = details::SrgbToLinear(g);
	const auto lb = details::SrgbToLinear(b);
	const auto l = std::cbrt(0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb);
	const auto m = std::cbrt(0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb);
	const auto s = std::cbrt(0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb);
	return {
		float(0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s),
		float(1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s),
		float(0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s),
	};
}

// Оттенок цвета в градусах OKLCH (для акцента темы).
[[nodiscard]] inline double HueOf(int r, int g, int b) {
	const auto lab = LabFromSrgb(r, g, b);
	auto degrees = std::atan2(double(lab.b), double(lab.a)) * 180. / details::kPi;
	return (degrees < 0.) ? (degrees + 360.) : degrees;
}

// Палитра из акцента темы: соседние оттенки, а не случайная радуга, поэтому
// фон всегда выглядит частью темы. Яркость берётся по тёмной или светлой теме.
[[nodiscard]] inline Palette ThemePalette(double accentHue, bool dark) {
	// Тёплые оттенки (оранжевый, жёлтый, оливковый) в тёмных тонах выглядят
	// коричневыми: для них берём светлее и чище, а разброс оттенков уже.
	const auto warm = dark && accentHue >= 30. && accentHue <= 125.;
	const auto spread = warm ? 0.6 : 1.;
	// Соседние оттенки: синий остаётся синим (раньше +48 уводило в фиолетовый).
	const double hues[4] = {
		accentHue,
		accentHue + 22. * spread,
		accentHue - 16. * spread,
		accentHue + 9. * spread,
	};
	if (warm) {
		static constexpr details::Profile kWarm = {
			{ 0.74, 0.62, 0.50, 0.36 },
			{ 0.15, 0.13, 0.11, 0.06 },
		};
		return details::Build(kWarm, hues);
	}
	return details::Build(dark ? details::kDark : details::kLight, hues);
}

// Готовые палитры тоже подстраиваются под яркость темы.
[[nodiscard]] inline Palette PresetPalette(Preset preset, bool dark) {
	switch (preset) {
	case Preset::Aurora: {
		const double hues[4] = { 190., 160., 270., 230. };
		return details::Build(dark ? details::kDark : details::kLight, hues);
	}
	case Preset::Sunset: {
		const double hues[4] = { 25., 45., 340., 300. };
		return details::Build(dark ? details::kDark : details::kLight, hues);
	}
	case Preset::Ocean: {
		const double hues[4] = { 230., 215., 255., 200. };
		return details::Build(dark ? details::kDark : details::kLight, hues);
	}
	case Preset::Graphite: {
		const double hues[4] = { 270., 270., 270., 270. };
		return details::Build(
			dark ? details::kDark : details::kLight,
			hues,
			0.12);
	}
	case Preset::Neon: {
		const double hues[4] = { 300., 350., 260., 200. };
		return details::Build(
			dark ? details::kDarkVivid : details::kLight,
			hues);
	}
	case Preset::Forest: {
		const double hues[4] = { 155., 140., 175., 125. };
		return details::Build(dark ? details::kDark : details::kLight, hues);
	}
	default: break;
	}
	return ThemePalette(250., dark);
}

// Приглушение: цвета смещаются к спокойному нейтральному фону темы.
[[nodiscard]] inline Palette Muted(Palette palette, bool dark, float amount) {
	if (amount <= 0.f) {
		return palette;
	}
	const auto base = Lab{ dark ? 0.18f : 0.97f, 0.f, 0.f };
	for (auto &c : palette) {
		c.L += (base.L - c.L) * amount;
		c.a += (base.a - c.a) * amount;
		c.b += (base.b - c.b) * amount;
	}
	return palette;
}

} // namespace Mogram::Gradient
