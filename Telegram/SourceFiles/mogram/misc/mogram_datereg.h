// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: a2756eed
// Mogram: дата регистрации аккаунта через API DateReg (https://docs.datereg.pro/api).
#pragma once

#include <rpl/producer.h>

#include "base/basic_types.h"

#include <QtCore/QString>

class PeerData;

namespace Mogram::Misc {


// Оценка по таблице опорных точек, без сети. Пусто, если regdate.show выключен.
// Для пользователя зажата самым ранним известным фото, для чата и канала это грубая оценка.
[[nodiscard]] QString RegdateText(not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<QString> RegdateValue(not_null<PeerData*> peer);

[[nodiscard]] bool DateRegEnabled();

// Заранее запрашивает дату, чтобы подсказка в профиле показалась сразу.
void Prefetch(quint64 userId);

// Подпись вида "январь 2024" (после загрузки). Пока ждём, отдаёт "…".
[[nodiscard]] rpl::producer<QString> RegistrationDate(quint64 userId);

} // namespace Mogram::Misc
