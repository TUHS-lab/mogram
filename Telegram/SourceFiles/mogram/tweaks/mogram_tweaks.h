// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: мелкие настройки: полоса историй, спойлеры, свои закрепы,
// метаданные файлов.
#pragma once

#include "base/basic_types.h"

class PeerData;
namespace Main {
class Session;
} // namespace Main

namespace Mogram::Tweaks {

[[nodiscard]] bool HideStories();
[[nodiscard]] bool RevealSpoilers();

// Закрепы только у себя, сервер о них не знает. 0 значит не закреплён,
// меньше число выше в списке.
[[nodiscard]] int LocalPinIndex(not_null<PeerData*> peer);
[[nodiscard]] bool IsLocallyPinned(not_null<PeerData*> peer);
void ToggleLocalPin(not_null<PeerData*> peer);
// Весь список закрепов (значения PeerId) и его замена целиком.
[[nodiscard]] QStringList LocalPinList();
void SetLocalPinList(not_null<Main::Session*> session, const QStringList &ids);

// Убирает из JPEG и PNG геотег, камеру, автора и время съёмки.
// Поворот кадра сохраняется. Другие форматы возвращаются как есть.
[[nodiscard]] QByteArray StripMetadata(const QByteArray &bytes);
[[nodiscard]] bool StripsMetadata();

void RegisterSettings();

} // namespace Mogram::Tweaks
