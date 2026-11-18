// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: change (fork): animated gradient background, pure math part.
// Считает маленький кадр сетчатого градиента из четырёх цветов: точки плавно
// ходят по замкнутым кривым, а поле слегка «течёт» (искажение координат), поэтому
// формы живые, а не круглые пятна. Смешивание в OKLab. Ничего из Qt.
#pragma once

#include "mogram/gradient/mogram_gradient_palette.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Mogram::Gradient {

// Самая длинная сторона кадра. Градиент гладкий, 128 точек хватает, а счёт
// остаётся в районе десяти тысяч пикселей на кадр.
inline constexpr auto kMeshSide = 128;

struct MeshSize {
	int width = 0;
	int height = 0;
};

[[nodiscard]] inline MeshSize MeshSizeFor(int fillWidth, int fillHeight) {
	if (fillWidth <= 0 || fillHeight <= 0) {
		return {};
	}
	if (fillWidth >= fillHeight) {
		const auto h = int(std::lround(
			double(kMeshSide) * fillHeight / fillWidth));
		return { kMeshSide, h < 2 ? 2 : h };
	}
	const auto w = int(std::lround(
		double(kMeshSide) * fillWidth / fillHeight));
	return { w < 2 ? 2 : w, kMeshSide };
}

namespace details {

constexpr auto kGammaSteps = 4096;

// Линейный свет в sRGB 0..255 через таблицу: pow на каждый пиксель дорог.
[[nodiscard]] inline const std::array<std::uint8_t, kGammaSteps + 1> &GammaTable() {
	static const auto table = [] {
		auto result = std::array<std::uint8_t, kGammaSteps + 1>();
		for (auto i = 0; i <= kGammaSteps; ++i) {
			const auto x = double(i) / kGammaSteps;
			const auto v = (x <= 0.0031308)
				? (12.92 * x)
				: (1.055 * std::pow(x, 1. / 2.4) - 0.055);
			result[i] = std::uint8_t(std::lround(v * 255.));
		}
		return result;
	}();
	return table;
}

[[nodiscard]] inline std::uint32_t ToSrgb8(
		const std::array<std::uint8_t, kGammaSteps + 1> &table,
		float linear) {
	if (linear <= 0.f) {
		return table[0];
	} else if (linear >= 1.f) {
		return table[kGammaSteps];
	}
	return table[int(linear * kGammaSteps + 0.5f)];
}

} // namespace details

// phase в радианах и только растёт: движение не зависит от размера окна.
// out: width * height пикселей 0xffRRGGBB.
inline void RenderMesh(
		std::uint32_t *out,
		int width,
		int height,
		const Palette &colors,
		double phase) {
	if (!out || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
		return;
	}
	// У каждой точки свои частоты и сдвиги, поэтому узор не повторяется быстро.
	constexpr double kFreqX[4] = { 0.91, 0.63, 1.13, 0.77 };
	constexpr double kFreqY[4] = { 0.71, 1.07, 0.59, 0.97 };
	constexpr double kOffX[4] = { 0.0, 1.9, 3.7, 5.1 };
	constexpr double kOffY[4] = { 2.3, 0.4, 4.4, 1.2 };
	constexpr double kRange = 0.46;
	constexpr auto kSoft = 0.034f;

	const auto aspect = double(width) / height;
	float px[4], py[4];
	for (auto i = 0; i != 4; ++i) {
		px[i] = float((0.5 + kRange * std::sin(phase * kFreqX[i] + kOffX[i])) * aspect);
		py[i] = float(0.5 + kRange * std::cos(phase * kFreqY[i] + kOffY[i]));
	}
	const auto invW = float(aspect / width);
	const auto invH = 1.f / height;

	// Искажение координат: сдвиг по x зависит только от строки, по y только
	// от столбца, так что считается один раз на кадр, а не на пиксель.
	auto rowShift = std::array<float, 4096>();
	auto colShift = std::array<float, 4096>();
	for (auto y = 0; y != height; ++y) {
		const auto ny = (y + 0.5) * invH;
		rowShift[y] = float(0.07 * std::sin(2.6 * ny + phase * 0.8 + 1.0)
			+ 0.025 * std::sin(5.1 * ny - phase * 0.5));
	}
	for (auto x = 0; x != width; ++x) {
		const auto nx = (x + 0.5) * invW;
		colShift[x] = float(0.07 * std::sin(1.84 * nx + phase * 0.7)
			+ 0.025 * std::sin(3.76 * nx + phase * 0.6));
	}

	const auto &gamma = details::GammaTable();
	for (auto y = 0; y != height; ++y) {
		const auto ny = (y + 0.5f) * invH;
		auto row = out + std::size_t(y) * width;
		for (auto x = 0; x != width; ++x) {
			const auto nx = (x + 0.5f) * invW + rowShift[y];
			const auto wy = ny + colShift[x];
			float w[4];
			auto sum = 0.f;
			for (auto i = 0; i != 4; ++i) {
				const auto dx = nx - px[i];
				const auto dy = wy - py[i];
				const auto q = dx * dx + dy * dy + kSoft;
				w[i] = 1.f / (q * q);
				sum += w[i];
			}
			const auto k = 1.f / sum;
			Lab mix;
			for (auto i = 0; i != 4; ++i) {
				mix.L += w[i] * colors[i].L;
				mix.a += w[i] * colors[i].a;
				mix.b += w[i] * colors[i].b;
			}
			mix.L *= k;
			mix.a *= k;
			mix.b *= k;
			float rgb[3];
			details::LabToLinear(mix, rgb);
			row[x] = 0xFF000000u
				| (details::ToSrgb8(gamma, rgb[0]) << 16)
				| (details::ToSrgb8(gamma, rgb[1]) << 8)
				| details::ToSrgb8(gamma, rgb[2]);
		}
	}
}

} // namespace Mogram::Gradient
