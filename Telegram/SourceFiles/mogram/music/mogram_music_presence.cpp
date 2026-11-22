// This file is part of Mogram, https://github.com/mogram/mogram.
// GPLv3 with the attribution term of MOGRAM_NOTICE.md.
#include "mogram/music/mogram_music_presence.h"

#include "base/random.h"
#include "base/timer.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "mogram/misc/mogram_misc.h"
#include "mogram/settings/mogram_settings.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"

#include "styles/palette.h"

#include <QtCore/QStringList>

namespace Mogram::Music::Presence {
namespace {

constexpr auto kLease = crl::time(35000);
constexpr auto kRefresh = crl::time(4000);
constexpr auto kPublishCheck = crl::time(1000);
constexpr auto kReadInterval = crl::time(2000);
constexpr auto kLabelBytes = 150;
constexpr auto kBatchSize = 12;
const auto kResultPrefix = u"presence-v1:"_q;
const auto kWriteAck = u"presence-v1"_q;

[[nodiscard]] QByteArray Encode(const Playback &track) {
	auto label = track.title.simplified();
	if (label.isEmpty()) {
		return {};
	}
	const auto artist = track.artist.simplified();
	if (!artist.isEmpty()) {
		label += u" · "_q + artist;
	}
	auto bytes = label.toUtf8();
	if (bytes.size() > kLabelBytes) {
		auto end = kLabelBytes;
		while ((uchar(bytes.at(end)) & 0xC0) == 0x80) {
			--end;
		}
		bytes.truncate(end);
	}
	const auto encoded = bytes.toBase64(
		QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	return track.soundcloudId
		? (QByteArray::number(track.soundcloudId) + ':' + encoded)
		: encoded;
}

struct Activity {
	Track track;
	crl::time expiresAt = 0;
	uint64 request = 0;
};

[[nodiscard]] Activity Decode(
		const Bot::Result &result,
		crl::time sentAt,
		crl::time now) {
	const auto description = QStringView(result.description);
	const auto separator = description.indexOf(u':');
	auto ok = false;
	const auto ttl = description.left(separator < 0
		? description.size()
		: separator).toInt(&ok);
	if (!ok || ttl <= 0 || ttl > kLease || sentAt + ttl <= now
		|| result.title.toUtf8().size() > kLabelBytes) {
		return {};
	}
	const auto label = result.title.simplified();
	const auto split = label.lastIndexOf(u" · "_q);
	const auto id = (separator < 0)
		? uint64(0)
		: description.mid(separator + 1).toULongLong();
	return {
		.track = {
			.title = split < 0 ? label : label.left(split),
			.artist = split < 0 ? QString() : label.mid(split + 3),
			.soundcloudId = id,
		},
		.expiresAt = sentAt + ttl,
	};
}

struct Publisher {
	const QString writer = QString::number(base::RandomValue<uint64>(), 16);
	Fn<Playback()> playback;
	base::weak_ptr<Main::Session> session;
	QByteArray last;
	crl::time refreshAt = 0;
	int request = 0;
	bool pending = false;
	bool shared = false;
	base::Timer timer;
};

void Refresh(const std::shared_ptr<Publisher> &state) {
	auto &s = *state;
	auto session = s.session.get();
	if (!session) {
		s.pending = s.shared = false;
		s.last.clear();
	}
	if (s.pending) {
		return;
	}
	const auto current = Settings::Bool(kShareKey) ? s.playback() : Playback();
	const auto label = current.session ? Encode(current) : QByteArray();
	const auto now = crl::now();
	const auto clear = s.shared && (session != current.session || label.isEmpty());
	if (!clear) {
		if (label.isEmpty() || (label == s.last && now < s.refreshAt)) {
			return;
		}
		session = current.session;
		s.session = base::make_weak(session);
		s.shared = true;
	}
	s.pending = true;
	s.last = clear ? QByteArray() : label;
	s.refreshAt = now + kRefresh;
	const auto request = ++s.request;
	const auto complete = [weak = std::weak_ptr(state), request, clear](
			bool acknowledged) {
		if (const auto state = weak.lock(); state && state->request == request) {
			state->pending = false;
			if (clear) {
				state->shared = false;
			} else if (!Settings::Bool(kShareKey)) {
				Refresh(state);
			} else if (!acknowledged) {
				state->refreshAt = crl::now() + kPublishCheck;
			}
		}
	};
	Bot::Ask(
		session,
		clear
			? u"presence:v1:clear:"_q + s.writer
			: u"presence:v1:put:"_q + s.writer + u':' + QString::fromLatin1(label),
		[=](Bot::Results results) {
			complete(results.size() == 1 && results.front().id == kWriteAck);
		},
		[=] { complete(false); });
}

struct Viewer {
	base::Timer poll;
	Tracks shown;
	Fn<void(Tracks)> changed;
	int generation = 0;
	int pending = 0;
};

struct ReadCache {
	base::Timer expiry;
	std::map<uint64, Activity> confirmed;
	rpl::event_stream<> changed;
	uint64 request = 0;
};

void Notify(ReadCache &cache) {
	const auto now = crl::now();
	std::erase_if(cache.confirmed, [&](const auto &entry) {
		return entry.second.expiresAt <= now;
	});
	auto next = crl::time(0);
	for (const auto &[id, activity] : cache.confirmed) {
		next = next ? std::min(next, activity.expiresAt) : activity.expiresAt;
	}
	cache.expiry.cancel();
	if (next) {
		cache.expiry.callOnce(next - now);
	}
	cache.changed.fire({});
}

[[nodiscard]] std::shared_ptr<ReadCache> ReadCacheFor(
		not_null<Main::Session*> session) {
	static auto caches = std::map<Main::Session*, std::shared_ptr<ReadCache>>();
	const auto [i, inserted] = caches.try_emplace(session.get());
	if (inserted) {
		const auto cache = i->second = std::make_shared<ReadCache>();
		cache->expiry.setCallback([weak = std::weak_ptr(cache)] {
			if (const auto cache = weak.lock()) {
				Notify(*cache);
			}
		});
		session->lifetime().add([=] { caches.erase(session.get()); });
	}
	return i->second;
}

} // namespace

void Publish(Fn<Playback()> playback, rpl::lifetime &lifetime) {
	const auto state = std::make_shared<Publisher>();
	state->playback = std::move(playback);
	const auto weak = std::weak_ptr(state);
	const auto refresh = [weak] {
		if (const auto state = weak.lock()) {
			Refresh(state);
		}
	};
	state->timer.setCallback(refresh);
	lifetime.add([state] {});
	Settings::BoolValue(kShareKey) | rpl::on_next([=](bool enabled) {
		if (const auto state = weak.lock()) {
			if (enabled) {
				state->timer.callEach(kPublishCheck);
			} else {
				state->timer.cancel();
			}
		}
		refresh();
	}, lifetime);
}

bool CanWatch(not_null<UserData*> user) {
	return !user->isSelf()
		&& !user->isBot()
		&& !user->isServiceUser()
		&& !user->isInaccessible()
		&& !user->isBlocked();
}

void Watch(
		not_null<UserData*> user,
		not_null<Ui::RpWidget*> view,
		Fn<void(Track)> changed,
		rpl::lifetime &lifetime) {
	if (!CanWatch(user)) {
		return;
	}
	const auto id = peerToUser(user->id).bare;
	WatchList(
		&user->session(),
		view,
		[=] {
			return CanWatch(user)
				? std::vector<uint64>{ id }
				: std::vector<uint64>();
		},
		[=](Tracks tracks) {
			const auto i = tracks.find(id);
			changed(i == tracks.end() || !CanWatch(user)
				? Track()
				: std::move(i->second));
		},
		lifetime);
}

void WatchList(
		not_null<Main::Session*> session,
		not_null<Ui::RpWidget*> view,
		Fn<std::vector<uint64>()> visible,
		Fn<void(Tracks)> changed,
		rpl::lifetime &lifetime) {
	const auto state = std::make_shared<Viewer>();
	state->changed = std::move(changed);
	const auto weak = std::weak_ptr(state);
	const auto cache = ReadCacheFor(session);
	const auto update = [=] {
		const auto state = weak.lock();
		if (!state) {
			return;
		}
		auto tracks = Tracks();
		if (view->isVisible() && Settings::Bool(kShowKey)) {
			const auto now = crl::now();
			for (const auto id : visible()) {
				const auto i = cache->confirmed.find(id);
				if (i != cache->confirmed.end() && i->second.track
					&& i->second.expiresAt > now) {
					tracks.emplace(id, i->second.track);
				}
			}
		}
		if (tracks != state->shown) {
			state->shown = tracks;
			state->changed(std::move(tracks));
		}
	};
	const auto refresh = [=] {
		const auto state = weak.lock();
		if (!state || !view->isVisible()
			|| !Settings::Bool(kShowKey)) {
			return;
		}
		update();
		if (state->pending) {
			return;
		}
		auto ids = visible();
		std::sort(ids.begin(), ids.end());
		ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
		const auto generation = state->generation;
		for (auto first = ids.cbegin(); first != ids.cend();) {
			const auto end = first + std::min<ptrdiff_t>(
				kBatchSize, ids.cend() - first);
			const auto batch = std::vector<uint64>(first, end);
			first = end;
			auto portions = QStringList();
			for (const auto id : batch) {
				portions.push_back(QString::number(id));
			}
			++state->pending;
			const auto sentAt = crl::now();
			const auto request = ++cache->request;
			Bot::Ask(
				session,
				u"presence:v1:list:"_q + portions.join(u','),
				[=](Bot::Results results) {
					const auto state = weak.lock();
					if (!state || state->generation != generation) {
						return;
					}
					--state->pending;
					const auto now = crl::now();
					for (const auto &result : results) {
						if (!result.id.startsWith(kResultPrefix)) {
							continue;
						}
						const auto id = QStringView(result.id)
							.mid(kResultPrefix.size()).toULongLong();
						if (!std::binary_search(batch.begin(), batch.end(), id)) {
							continue;
						}
						const auto absent = (result.description == u"0"_q);
						auto activity = absent
							? Activity{ .track = {}, .expiresAt = sentAt + kLease }
							: Decode(result, sentAt, now);
						const auto i = cache->confirmed.find(id);
						const auto newer = (i == cache->confirmed.end())
							|| (i->second.request < request);
						if (newer && activity.expiresAt > now && (activity.track || absent)) {
							activity.request = request;
							cache->confirmed.insert_or_assign(id, std::move(activity));
						}
					}
					Notify(*cache);
				},
				[=] {
					if (const auto state = weak.lock();
						state && state->generation == generation) {
						--state->pending;
					}
				});
		}
	};
	state->poll.setCallback(refresh);
	lifetime.add([state] {});
	cache->changed.events() | rpl::on_next(update, lifetime);
	view->events() | rpl::filter([](not_null<QEvent*> event) {
		return event->type() == QEvent::Show;
	}) | rpl::on_next(refresh, lifetime);
	Settings::BoolValue(kShowKey) | rpl::on_next([=](bool enabled) {
		if (const auto state = weak.lock()) {
			if (enabled) {
				state->poll.callEach(kReadInterval);
				refresh();
			} else {
				++state->generation;
				state->pending = 0;
				state->poll.cancel();
				if (cache->confirmed.empty()) {
					update();
				} else {
					cache->confirmed.clear();
					Notify(*cache);
				}
			}
		}
	}, lifetime);
}

void PaintRing(QPainter &p, const QRect &avatar) {
	p.save();
	auto hq = PainterHighQualityEnabler(p);
	const auto rect = QRectF(avatar);
	const auto line = style::ConvertFloatScale(2.);
	const auto inset = line / 2.;
	const auto path = rect.adjusted(inset, inset, -inset, -inset);

	p.setPen(QPen(st::windowBgActive->c, line));
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(path);
	p.restore();
}

} // namespace Mogram::Music::Presence
