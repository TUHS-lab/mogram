// This file is part of Mogram, https://github.com/mogram/mogram
// (c) 2026 Mogram contributors. GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#include "mogram/music/mogram_youtube_music.h"

#include "mogram/lang/mogram_lang.h"
#include "mogram/music/mogram_soundcloud.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <algorithm>

namespace Mogram::Music {
namespace {

constexpr auto kOrigin = "https://music.youtube.com";
constexpr auto kSongFilter = "Eg-KAQwIARAAGAAgACgAMABqChAEEAMQCRAFEAo%3D";
constexpr auto kResponseLimit = 4 * 1024 * 1024;
constexpr auto kAudioLimit = 100 * 1024 * 1024;
constexpr auto kAudioExtensions = { "m4a", "webm" };

struct CatalogConfig {
	QString key;
	QJsonObject client;
	qint64 expires = 0;
};

CatalogConfig &Config() {
	static auto config = CatalogConfig();
	return config;
}

void CatalogRequest(const QUrl &url, QJsonObject body, Fn<void(QByteArray)> done) {
	static const auto net = new QNetworkAccessManager(QCoreApplication::instance());
	auto request = QNetworkRequest(url);
	request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");
	request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
	request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
	request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
	request.setTransferTimeout(10000);
	const auto bootstrap = body.isEmpty();
	if (!bootstrap) {
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
		request.setRawHeader("Origin", kOrigin);
	}
	const auto reply = bootstrap
		? net->get(request)
		: net->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::redirected, reply, [=](const QUrl &target) {
		const auto host = target.host();
		if (target.scheme() == u"https"_q && target.port(443) == 443 && target.userInfo().isEmpty()
			&& (host == u"music.youtube.com"_q || (bootstrap && host == u"consent.youtube.com"_q))) {
			reply->redirectAllowed();
		} else {
			reply->abort();
		}
	});
	reply->setReadBufferSize(kResponseLimit + 1);
	QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [=](qint64 received, qint64 total) {
		if (received > kResponseLimit || total > kResponseLimit) reply->abort();
	});
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		const auto data = reply->error() == QNetworkReply::NoError
			? reply->readAll()
			: QByteArray();
		reply->deleteLater();
		done(data.size() <= kResponseLimit ? data : QByteArray());
	});
}

void LoadCatalogConfig(Fn<void(CatalogConfig)> done) {
	const auto now = QDateTime::currentSecsSinceEpoch();
	if (Config().expires > now) {
		done(Config());
		return;
	}
	CatalogRequest(QUrl(QString::fromLatin1(kOrigin) + u"/?hl=en"_q), {}, [=](QByteArray page) {
		const auto html = QString::fromUtf8(page);
		const auto field = [&](const QString &name) {
			return QRegularExpression(u"\"%1\"\\s*:\\s*\"([^\"]+)\""_q.arg(name))
				.match(html).captured(1);
		};
		const auto key = field(u"INNERTUBE_API_KEY"_q);
		const auto name = field(u"INNERTUBE_CLIENT_NAME"_q);
		const auto version = field(u"INNERTUBE_CLIENT_VERSION"_q);
		if (key.isEmpty() || name.isEmpty() || version.isEmpty()) {
			done({});
			return;
		}
		Config() = {
			.key = key,
			.client = QJsonObject{
				{ "clientName", name },
				{ "clientVersion", version },
				{ "visitorData", field(u"VISITOR_DATA"_q) },
				{ "hl", "en" },
			},
			.expires = now + 6 * 60 * 60,
		};
		done(Config());
	});
}

[[nodiscard]] QString MatchText(QString text) {
	static const auto separators = QRegularExpression(u"[^\\p{L}\\p{N}]+"_q);
	return text.normalized(QString::NormalizationForm_KC).toCaseFolded().remove(separators);
}

[[nodiscard]] int ReadSeconds(const QString &value) {
	static const auto duration = QRegularExpression(u"^(?:(\\d{1,3}):)?(\\d{1,3}):([0-5]\\d)$"_q);
	const auto match = duration.match(value);
	return match.hasMatch()
		? match.captured(1).toInt() * 3600 + match.captured(2).toInt() * 60 + match.captured(3).toInt()
		: 0;
}

[[nodiscard]] QString VideoType(const QJsonObject &object) {
	return object.value("navigationEndpoint").toObject()
		.value("watchEndpoint").toObject()
		.value("watchEndpointMusicSupportedConfigs").toObject()
		.value("watchEndpointMusicConfig").toObject()
		.value("musicVideoType").toString();
}

struct Match {
	QString id;
	int delta = 13;
};

void FindSong(const QJsonValue &value, const QString &title, const QString &artist, int seconds, Match &best) {
	if (best.delta == 0) return;
	if (value.isArray()) {
		for (const auto &child : value.toArray()) FindSong(child, title, artist, seconds, best);
		return;
	}
	const auto object = value.toObject();
	const auto row = object.value("musicResponsiveListItemRenderer").toObject();
	if (row.isEmpty()) {
		for (auto i = object.constBegin(); i != object.constEnd(); ++i) {
			FindSong(i.value(), title, artist, seconds, best);
		}
		return;
	}
	static const auto videoId = QRegularExpression(u"^[A-Za-z0-9_-]{11}$"_q);
	const auto id = row.value("playlistItemData").toObject().value("videoId").toString();
	const auto type = VideoType(row);
	if (!videoId.match(id).hasMatch() || (!type.isEmpty() && type != u"MUSIC_VIDEO_TYPE_ATV"_q)) return;
	const auto columns = row.value("flexColumns").toArray();
	if (columns.size() < 2) return;
	const auto runs = [&](int index) {
		return columns.at(index).toObject().value("musicResponsiveListItemFlexColumnRenderer")
			.toObject().value("text").toObject().value("runs").toArray();
	};
	auto name = QString();
	for (const auto &entry : runs(0)) {
		const auto run = entry.toObject();
		const auto type = VideoType(run);
		if (!type.isEmpty() && type != u"MUSIC_VIDEO_TYPE_ATV"_q) return;
		name += run.value("text").toString();
	}
	if (MatchText(name) != title) return;
	auto credits = QStringList();
	auto firstCredit = QString();
	auto duration = 0;
	for (const auto &entry : runs(1)) {
		const auto run = entry.toObject();
		const auto text = run.value("text").toString().trimmed();
		const auto channel = run.value("navigationEndpoint").toObject()
			.value("browseEndpoint").toObject().value("browseId").toString();
		if (channel.startsWith(u"UC"_q)) credits.push_back(text);
		if (const auto length = ReadSeconds(text)) {
			duration = length;
		} else if (firstCredit.isEmpty() && text != u"Song"_q && !MatchText(text).isEmpty()) {
			firstCredit = text;
		}
	}
	if (MatchText(credits.empty() ? firstCredit : credits.join(u", "_q)) != artist) return;
	if (seconds > 0 && duration == 0) return;
	const auto delta = seconds > 0 ? std::abs(duration - seconds) : 0;
	if (delta < best.delta) best = { id, delta };
}

[[nodiscard]] QString RowVideoId(const QJsonObject &row) {
	static const auto valid = QRegularExpression(u"^[A-Za-z0-9_-]{11}$"_q);
	auto id = row.value("playlistItemData").toObject()
		.value("videoId").toString();
	if (!valid.match(id).hasMatch()) {
		id = row.value("navigationEndpoint").toObject()
			.value("watchEndpoint").toObject()
			.value("videoId").toString();
	}
	return valid.match(id).hasMatch() ? id : QString();
}

[[nodiscard]] QString RowThumbnail(const QJsonObject &row) {
	const auto list = row.value("thumbnail").toObject()
		.value("musicThumbnailRenderer").toObject()
		.value("thumbnail").toObject()
		.value("thumbnails").toArray();
	auto result = QString();
	auto bestWidth = -1;
	for (const auto &value : list) {
		const auto thumb = value.toObject();
		const auto url = thumb.value("url").toString();
		const auto width = thumb.value("width").toInt();
		if (!url.isEmpty() && width >= bestWidth) {
			bestWidth = width;
			result = url;
		}
	}
	return result;
}

void CollectArtistSongs(
		const QJsonValue &value,
		const QString &artist,
		int limit,
		std::vector<Track> &result) {
	if (int(result.size()) >= limit) {
		return;
	} else if (value.isArray()) {
		for (const auto &child : value.toArray()) {
			CollectArtistSongs(child, artist, limit, result);
			if (int(result.size()) >= limit) {
				return;
			}
		}
		return;
	}
	const auto object = value.toObject();
	const auto row = object.value("musicResponsiveListItemRenderer").toObject();
	if (row.isEmpty()) {
		for (auto i = object.constBegin(); i != object.constEnd(); ++i) {
			CollectArtistSongs(i.value(), artist, limit, result);
			if (int(result.size()) >= limit) {
				return;
			}
		}
		return;
	}
	const auto id = RowVideoId(row);
	if (id.isEmpty()) {
		return;
	}
	const auto type = VideoType(row);
	if (!type.isEmpty() && type != u"MUSIC_VIDEO_TYPE_ATV"_q) {
		return;
	}
	const auto columns = row.value("flexColumns").toArray();
	if (columns.size() < 2) {
		return;
	}
	const auto runs = [&](int index) {
		return columns.at(index).toObject()
			.value("musicResponsiveListItemFlexColumnRenderer")
			.toObject().value("text").toObject().value("runs").toArray();
	};
	auto title = QString();
	for (const auto &entry : runs(0)) {
		const auto run = entry.toObject();
		const auto runType = VideoType(run);
		if (!runType.isEmpty() && runType != u"MUSIC_VIDEO_TYPE_ATV"_q) {
			return;
		}
		title += run.value("text").toString();
	}
	if (MatchText(title).isEmpty()) {
		return;
	}
	auto credits = QStringList();
	auto firstCredit = QString();
	auto duration = 0;
	for (const auto &entry : runs(1)) {
		const auto run = entry.toObject();
		const auto text = run.value("text").toString().trimmed();
		const auto channel = run.value("navigationEndpoint").toObject()
			.value("browseEndpoint").toObject().value("browseId").toString();
		if (channel.startsWith(u"UC"_q)) {
			credits.push_back(text);
		}
		if (const auto length = ReadSeconds(text)) {
			duration = length;
		} else if (firstCredit.isEmpty()
			&& text != u"Song"_q
			&& !MatchText(text).isEmpty()) {
			firstCredit = text;
		}
	}
	const auto artistKey = MatchText(artist);
	auto matched = std::any_of(credits.begin(), credits.end(), [&](const QString &credit) {
		return MatchText(credit) == artistKey;
	});
	if (!matched && credits.isEmpty()) {
		matched = (MatchText(firstCredit) == artistKey);
	}
	if (!matched) {
		return;
	}
	const auto known = std::any_of(result.begin(), result.end(), [&](const Track &track) {
		return track.videoId == id;
	});
	if (known) {
		return;
	}
	auto track = Track();
	track.id = u"yt:"_q + id;
	track.videoId = id;
	track.title = title.trimmed();
	track.artist = credits.empty() ? firstCredit : credits.join(u", "_q);
	track.artworkUrl = RowThumbnail(row);
	if (track.artworkUrl.isEmpty()) {
		track.artworkUrl = u"https://i.ytimg.com/vi/"_q + id + u"/hqdefault.jpg"_q;
	}
	track.permalink = QString::fromLatin1(kOrigin) + u"/watch?v="_q + id;
	track.durationMs = duration * 1000;
	track.uploaderName = artist;
	track.unavailable = true;
	result.push_back(std::move(track));
}

void SearchSong(const Track &track, Fn<void(QString)> done) {
	LoadCatalogConfig([=](CatalogConfig config) {
		if (config.key.isEmpty()) {
			done({});
			return;
		}
		auto url = QUrl(QString::fromLatin1(kOrigin) + u"/youtubei/v1/search"_q);
		auto query = QUrlQuery();
		query.addQueryItem(u"alt"_q, u"json"_q);
		query.addQueryItem(u"key"_q, config.key);
		url.setQuery(query);
		CatalogRequest(url, QJsonObject{
			{ "context", QJsonObject{ { "client", config.client } } },
			{ "query", (track.artist + u' ' + track.title).simplified() },
			{ "params", QString::fromLatin1(kSongFilter) },
		}, [=](QByteArray data) {
			if (data.isEmpty()) Config() = {};
			const auto contents = QJsonDocument::fromJson(data).object().value("contents");
			auto match = Match();
			FindSong(contents, MatchText(track.title), MatchText(track.artist), track.durationMs / 1000, match);
			done(match.id);
		});
	});
}

void LoadArtistTracksImpl(
		const QString &artist,
		int limit,
		Fn<void(std::vector<Track>)> done) {
	const auto name = artist.trimmed();
	if (name.isEmpty() || limit <= 0) {
		done({});
		return;
	}
	LoadCatalogConfig([=](CatalogConfig config) {
		if (config.key.isEmpty()) {
			done({});
			return;
		}
		auto url = QUrl(QString::fromLatin1(kOrigin) + u"/youtubei/v1/search"_q);
		auto query = QUrlQuery();
		query.addQueryItem(u"alt"_q, u"json"_q);
		query.addQueryItem(u"key"_q, config.key);
		url.setQuery(query);
		CatalogRequest(url, QJsonObject{
			{ "context", QJsonObject{ { "client", config.client } } },
			{ "query", name },
			{ "params", QString::fromLatin1(kSongFilter) },
		}, [=](QByteArray data) {
			if (data.isEmpty()) Config() = {};
			auto result = std::vector<Track>();
			CollectArtistSongs(
				QJsonDocument::fromJson(data).object().value("contents"),
				name,
				limit,
				result);
			done(std::move(result));
		});
	});
}

[[nodiscard]] QString FindTool(const QString &name) {
	const auto data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	const auto scoped = QStandardPaths::findExecutable(name, {
		QCoreApplication::applicationDirPath(),
		data + u"/Mogram/tools"_q,
	});
	if (!scoped.isEmpty()) return scoped;
	const auto onPath = QStandardPaths::findExecutable(name);
	if (!onPath.isEmpty()) return onPath;
#ifdef Q_OS_MAC
	return QStandardPaths::findExecutable(name, {
		u"/opt/homebrew/bin"_q, u"/usr/local/bin"_q, u"/opt/local/bin"_q,
	});
#else // Q_OS_MAC
	return {};
#endif // Q_OS_MAC
}

[[nodiscard]] QString CachedAudio(const QString &prefix) {
	for (const auto extension : kAudioExtensions) {
		const auto file = QFileInfo(prefix + QString::fromLatin1(extension));
		if (file.isFile() && file.size() > 0 && file.size() <= kAudioLimit) return file.filePath();
	}
	return {};
}

[[nodiscard]] QString DownloadError() {
	return QString::fromUtf8(Lang::Tr("Не удалось воспроизвести трек из YouTube Music"));
}

void DownloadSong(const QString &tool, const QString &id, const QString &cachePrefix, AudioFileDone done) {
	QDir().mkpath(CacheDir());
	const auto temp = std::make_shared<QTemporaryDir>(CacheDir() + u"/yt-XXXXXX"_q);
	if (!temp->isValid()) {
		done({}, DownloadError());
		return;
	}
	const auto process = new QProcess(QCoreApplication::instance());
	process->setStandardOutputFile(QProcess::nullDevice());
	process->setStandardErrorFile(QProcess::nullDevice());
	QObject::connect(process, &QProcess::errorOccurred, process, [=](QProcess::ProcessError error) {
		if (error == QProcess::FailedToStart) {
			process->deleteLater();
			done({}, DownloadError());
		}
	});
	QObject::connect(process, &QProcess::finished, process, [=](int code, QProcess::ExitStatus status) {
		process->deleteLater();
		const auto file = code == 0 && status == QProcess::NormalExit
			? CachedAudio(temp->filePath(u"audio."_q))
			: QString();
		if (file.isEmpty()) {
			done({}, DownloadError());
			return;
		}
		if (const auto cached = CachedAudio(cachePrefix); !cached.isEmpty()) {
			done(cached, {});
			return;
		}
		const auto path = cachePrefix + QFileInfo(file).suffix();
		const auto existing = QFileInfo(path);
		if (existing.isFile() && (existing.size() == 0 || existing.size() > kAudioLimit)) {
			QFile::remove(path);
		}
		const auto saved = QFile::rename(file, path) ? path : CachedAudio(cachePrefix);
		done(saved, saved.isEmpty() ? DownloadError() : QString());
	});
	QTimer::singleShot(60000, process, [=] { process->kill(); });
	auto args = QStringList{
		u"--ignore-config"_q, u"--no-plugin-dirs"_q, u"--no-playlist"_q,
		u"--no-progress"_q, u"--socket-timeout"_q, u"10"_q, u"--retries"_q, u"1"_q,
		u"--max-filesize"_q, u"100M"_q, u"--check-formats"_q,
		u"--format"_q, u"bestaudio[ext=m4a]/bestaudio[ext=webm]"_q,
		u"--output"_q, temp->filePath(u"audio.%(ext)s"_q),
	};
	for (const auto &runtime : { u"deno"_q, u"node"_q }) {
		const auto path = FindTool(runtime);
		if (!path.isEmpty()) args << u"--js-runtimes"_q << (runtime + u':' + path);
	}
	args << (QString::fromLatin1(kOrigin) + u"/watch?v="_q + id);
	process->start(tool, args, QIODevice::ReadOnly);
}

} // namespace

void LoadArtistTracks(
		const QString &artist,
		int limit,
		Fn<void(std::vector<Track>)> done) {
	LoadArtistTracksImpl(artist, limit, std::move(done));
}

bool HasYouTubeMusicMetadata(const Track &track) {
	return (track.id.toULongLong() || !track.videoId.isEmpty())
		&& !MatchText(track.title).isEmpty()
		&& !MatchText(track.artist).isEmpty();
}

void DownloadYouTubeMusic(const Track &track, AudioFileDone done) {
	if (!HasYouTubeMusicMetadata(track)) {
		done({}, DownloadError());
		return;
	}
	const auto known = !track.videoId.isEmpty();
	const auto key = known
		? track.videoId
		: QString::number(track.id.toULongLong());
	const auto prefix = (known ? u"/yt-v-"_q : u"/yt-"_q) + key + u'.';
	LoadCachedAudio(prefix, CachedAudio(prefix), [=](AudioFileDone finish) {
		const auto tool = FindTool(u"yt-dlp"_q);
		if (tool.isEmpty()) {
			finish({}, QString::fromUtf8(Lang::Tr("Для воспроизведения из YouTube Music установите yt-dlp")));
			return;
		}
		if (known) {
			DownloadSong(tool, track.videoId, prefix, finish);
			return;
		}
		SearchSong(track, [=](QString videoId) {
			if (videoId.isEmpty()) {
				finish({}, DownloadError());
				return;
			}
			DownloadSong(tool, videoId, prefix, finish);
		});
	}, std::move(done));
}

} // namespace Mogram::Music
