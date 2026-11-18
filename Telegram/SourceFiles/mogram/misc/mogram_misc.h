// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 4961283f
// Mogram: мелочи (лимит аккаунтов, стикер из фото, владелец пака, название клиента).
#pragma once

#include "base/basic_types.h"

#include <QtCore/QByteArray>
#include <memory>
#include <vector>
#include <QtCore/QString>
#include <QtCore/QTime>
#include <QtGui/QIcon>
#include <QtGui/QImage>

class PhotoData;
class PeerData;
class HistoryItem;
class ClickHandler;

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Mogram::Misc {

// Регистрирует настройки раздела «Аккаунты» (misc.max_accounts).
void RegisterSettings();

// Пункты контекстного меню кнопки бота: текст, callback data, url, inline query.
void AddBotButtonCopyActions(
	not_null<Ui::PopupMenu*> menu,
	const std::shared_ptr<ClickHandler> &link);

// adblock.hideSponsored: спонсорские сообщения не запрашиваются и не показываются.
[[nodiscard]] bool SponsoredHidden();
void NoteSponsoredBlocked(not_null<PeerData*> peer);

// Время в подвале сообщения: «12:34:56» при time.showSeconds, иначе как в Telegram.
[[nodiscard]] QString FormatMessageTime(const QTime &time);

// Выбранная в настройках иконка приложения; пустая, если стоит стандартная.
[[nodiscard]] QIcon CustomAppIcon();

// Картинка варианта иконки для списка выбора: 0 стандартная, 1..5 свои.
[[nodiscard]] QImage AppIconImage(int index);

// ДЦ пользователя, запомненный по его аватарке, пока аккаунт был живой.
// Нужен, когда аккаунт удалили или фото больше нет. 0 если не знаем.
void RememberUserDc(uint64 userId, int dcId);
[[nodiscard]] int RememberedUserDc(uint64 userId);

// Кладёт в буфер «[дата время] автор: текст» для сообщения.
void CopyWithAuthor(not_null<HistoryItem*> item);

// Вероятный id владельца набора по id набора. Это эвристика, а не контракт Telegram.
[[nodiscard]] uint64 OwnerIdFromStickerSetId(uint64 setId);

// Открывает профиль вероятного владельца, а если его нет в кэше, показывает окно с ID.
void ShowStickerSetOwner(
	std::shared_ptr<ChatHelpers::Show> show,
	uint64 setId);

// Заменяет «Telegram Desktop» на «Mogram» в значении строки языка.
// Повторный вызов безопасен (замена идемпотентна).
[[nodiscard]] QByteArray BrandRewrite(const QByteArray &value);

// Путь отладочного файла в личном каталоге пользователя (XDG_RUNTIME_DIR, 0700).
// Пустая строка, если такого каталога нет: общий /tmp для этого не годится.
[[nodiscard]] QString DebugFilePath(const QString &name);

} // namespace Mogram::Misc

namespace Mogram::Bot {

// Ответ бота на inline-запрос: пары (id результата, заголовок).
struct Result {
	QString id;
	QString title;
	QString description;
};
using Results = std::vector<Result>;

// Спрашивает @moggent inline-запросом (без сообщений в чатах, нужна только сессия).
// fail вызывается, если нет сессии, бот не нашёлся или запрос не прошёл.
void Ask(const QString &query, Fn<void(Results)> done, Fn<void()> fail = nullptr);

} // namespace Mogram::Bot

namespace Mogram::Guard {

// Человек из списка исключений: проверка угроз на него не действует.
[[nodiscard]] bool ThreatExempt(not_null<PeerData*> peer);
void ToggleThreatExempt(not_null<PeerData*> peer);

} // namespace Mogram::Guard
