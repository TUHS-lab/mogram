// This file is part of Mogram, https://github.com/mogram/mogram
// (c) 2026 Mogram contributors. GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

#include <vector>

namespace Mogram::Music {

struct Track;

[[nodiscard]] bool HasYouTubeMusicMetadata(const Track &track);

// Популярные песни исполнителя из каталога YouTube Music. Возвращает
// неиграемые напрямую Track с videoId: плеер скачивает их через yt-dlp.
void LoadArtistTracks(
	const QString &artist,
	int limit,
	Fn<void(std::vector<Track>)> done);

// Запускается в главном потоке. Возвращает путь к аудио в кэше.
// Скачанный файл живёт в общем LRU кэше аудио, повторные запросы объединяются.
void DownloadYouTubeMusic(
	const Track &track,
	Fn<void(const QString &path, const QString &error)> done);

} // namespace Mogram::Music
