// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: c380595c
// Mogram: общие детали окон в стиле Mogram.
#pragma once

namespace Ui {
class AbstractButton;
class BoxContent;
class RpWidget;
} // namespace Ui

namespace style {
struct Box;
struct InputField;
struct RoundButton;
} // namespace style

namespace Mogram::BoxParts {

inline constexpr auto kBoxRadius = 28;
inline constexpr auto kCardRadius = 22;
inline constexpr auto kOuterSide = 14;
inline constexpr auto kRoundSize = 36;

enum class RoundIcon {
	Close,
	Back,
	Menu,
};

[[nodiscard]] not_null<Ui::AbstractButton*> MakeRoundButton(
	not_null<QWidget*> parent,
	RoundIcon icon,
	Fn<void()> callback);

[[nodiscard]] not_null<Ui::AbstractButton*> MakeSwitch(
	not_null<QWidget*> parent, rpl::producer<bool> value);
[[nodiscard]] not_null<Ui::AbstractButton*> MakeSwitch(
	not_null<QWidget*> parent, bool checked, Fn<void(bool)> changed,
	rpl::producer<bool> active = rpl::single(true));
void MakePill(not_null<Ui::RpWidget*> widget, Fn<QColor()> pageBg);

void RoundLayer(not_null<Ui::BoxContent*> box);
void PaintCard(QPainter &p, QRect rect);

[[nodiscard]] const style::Box &PillButtonsBox();
[[nodiscard]] const style::RoundButton &PillActive();
[[nodiscard]] const style::RoundButton &PillLight();
[[nodiscard]] const style::RoundButton &PillAttention();
[[nodiscard]] const style::InputField &CardField(
	const style::InputField &base);

} // namespace Mogram::BoxParts
