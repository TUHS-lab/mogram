/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/buttons.h"
#include <rpl/event_stream.h>
#include "ui/text/format_song_name.h"
#include "ui/text/text.h"

namespace Info::Profile {

class MusicButton;

// Mogram: плашка музыки внутри заданного виджета (если показана).
[[nodiscard]] MusicButton *FindMusicButton(not_null<QWidget*> inside);

// Mogram: музыка прилегает к шапке, пока показана хотя бы одна плашка.
[[nodiscard]] bool MusicShown();
// Плашка стоит прямо под нижним краем шапки (не прокручена в сторону).
[[nodiscard]] bool MusicAttachedTo(not_null<QWidget*> bar);
[[nodiscard]] std::optional<QColor> MusicBandColor();
[[nodiscard]] rpl::producer<> MusicShownChanges();
// Насколько развёрнута шапка профиля (1 развёрнута, 0 свёрнута). Плашка
// музыки плавно исчезает вместе со сворачиванием шапки.
void SetMusicFade(float64 value);
[[nodiscard]] float64 MusicFade();
[[nodiscard]] rpl::producer<> MusicFadeChanges();
// Плашка музыки стоит отдельной таблеткой под шапкой, а не вдвинута под её
// скруглённые края (свёрнутая шапка, шапка с цветным градиентом).
void SetMusicDetached(bool value);
[[nodiscard]] bool MusicDetached();

struct MusicButtonData {
	Ui::Text::FormatSongName name;
};

class MusicButton final : public Ui::RippleButton {
public:
	MusicButton(QWidget *parent, MusicButtonData data, Fn<void()> handler);
	~MusicButton();

	void updateData(MusicButtonData data);
	void setOverrideBg(std::optional<QColor> color);

	// Рисует плашку в произвольном painter (для липкой копии под шапкой).
	void paintContent(QPainter &p);
	// Плашку закрыла липкая копия: сама она не рисуется.
	void setCovered(bool covered);

private:
	void paintEvent(QPaintEvent *e) override;
	void setCounted(bool counted);
	void showEvent(QShowEvent *e) override;
	void visibleTopBottomUpdated(int visibleTop, int visibleBottom) override;
	void hideEvent(QHideEvent *e) override;
	int resizeGetHeight(int newWidth) override;

	Ui::Text::String _performer;
	Ui::Text::String _title;
	std::optional<QColor> _overrideBg;
	int _lastVisibleTop = -1000;
	int _visibleTop = 0;

	const QString _noteSymbol;
	const int _noteWidth;
	bool _counted = false;
	bool _covered = false;

};

// Mogram: копия плашки, прилипшая под свёрнутой шапкой профиля: содержимое
// уезжает под неё при прокрутке, а не обрезается по краю шапки.
class MusicSticky final : public Ui::RpWidget {
public:
	explicit MusicSticky(QWidget *parent);
	~MusicSticky();

	void setButton(MusicButton *button);

private:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

	QPointer<MusicButton> _button;
	bool _pressed = false;

};

} // namespace Info::Profile
