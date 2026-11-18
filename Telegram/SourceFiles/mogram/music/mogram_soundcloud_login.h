// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 3e91a4c7
// Mogram: вход в SoundCloud через обычный браузер.
// Встроенный вебвью в клиенте не работает. На Linux и macOS токен забирается
// из профиля браузера, вручную его вводят только если автоматика не сработала.
#pragma once

class QPainter;
class QRectF;

namespace Mogram::Music {

// Показывает окно входа: автовход через браузер и ручной ввод токена.
// onLoggedIn зовётся в главном потоке после успешного входа.
void ShowSoundCloudLogin(Fn<void()> onLoggedIn, Fn<void()> onClosed = nullptr);

// Тихо берёт свежий токен из куков браузера (Linux/macOS) и входит с ним.
// Токен SoundCloud живёт сутки, потом лайки отвечают 401.
void RefreshSoundCloudToken(Fn<void(bool)> done);

// Круглый оранжевый значок SoundCloud со знаком: облако и столбики звука.
void PaintSoundCloudBadge(QPainter &p, const QRectF &r);

// Открыть страницу SoundCloud в системном браузере.
[[nodiscard]] bool OpenSoundCloudInBrowser();

} // namespace Mogram::Music
