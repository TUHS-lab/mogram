// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: Mogram Link, связь ПК и телефона через закрытый канал в вашем аккаунте.
// Протокол MGL1:
//   текст сообщения = "⁣mgl1:" + base64url(iv[12] + AES-256-GCM(json) + tag[16]),
//   ключ = PBKDF2-HMAC-SHA1(пароль связи, соль "mgl1:" + id пользователя, 200000, 32 байта).
// Типы (поле t): clip, now, act, state. Поле d это id устройства-отправителя.
#include "mogram/link/mogram_link.h"

#include "mogram/lang/mogram_lang.h"
#include "mogram/music/mogram_music_ui.h"
#include "mogram/notes/mogram_notes.h"
#include "mogram/secure/mogram_secure.h"
#include "mogram/settings/mogram_settings.h"
#include "mogram/tweaks/mogram_tweaks.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "base/timer.h"
#include "core/application.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "dialogs/dialogs_key.h"
#include "window/main_window.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "media/player/media_player_instance.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUuid>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace Mogram::Link {
namespace {

constexpr auto kChannelTitle = "Mogram Link";
constexpr auto kMark = "\xE2\x81\xA3mgl1:";
constexpr auto kEventTtl = crl::time(90000);
constexpr auto kPbkdfRounds = 200000;

[[nodiscard]] QString U(const char *text) {
	return QString::fromUtf8(text);
}

[[nodiscard]] bool Flag(const char *name, bool fallback = true) {
	const auto key = u"link."_q + QString::fromUtf8(name);
	return Settings::Bool(key);
	Q_UNUSED(fallback);
}

[[nodiscard]] bool Enabled() {
	return Settings::Bool(u"link.enabled"_q);
}

[[nodiscard]] QString Password() {
	const auto stored = Settings::String(u"link.password"_q).toUtf8();
	return QString::fromUtf8(Secure::Open(QByteArray::fromBase64(stored)));
}

[[nodiscard]] QString DeviceId() {
	auto id = Settings::String(u"link.device"_q);
	if (id.isEmpty()) {
		id = u"p"_q + QUuid::createUuid().toString(QUuid::Id128).left(10);
		Settings::SetString(u"link.device"_q, id);
	}
	return id;
}

struct State {
	Main::Session *session = nullptr;
	ChannelId channel = 0;
	bool creating = false;
	QByteArray key;
	QString keyFor;
	QString lastClipHash;
	QString lastNowKey;
	base::flat_map<quint64, crl::time> remoteActive;
	base::Timer stateTimer;
	base::Timer nowTimer;
	base::Timer pulseTimer;
	qint64 pulseChat = 0;
	crl::time pulseAt = 0;
	std::vector<quint64> candidates;
	rpl::lifetime lifetime;
	rpl::lifetime appLifetime;
	bool started = false;
};

[[nodiscard]] State &Data() {
	static auto result = State();
	return result;
}

// --- шифрование ---

[[nodiscard]] QByteArray DeriveKey() {
	auto &state = Data();
	const auto password = Password();
	const auto user = state.session ? state.session->userId().bare : 0;
	const auto id = password + u"|"_q + QString::number(user);
	if (state.key.isEmpty() || state.keyFor != id) {
		const auto salt = QByteArray("mgl1:") + QByteArray::number(qulonglong(user));
		const auto pass = password.toUtf8();
		auto out = QByteArray(32, Qt::Uninitialized);
		PKCS5_PBKDF2_HMAC_SHA1(
			pass.constData(),
			int(pass.size()),
			reinterpret_cast<const unsigned char*>(salt.constData()),
			int(salt.size()),
			kPbkdfRounds,
			32,
			reinterpret_cast<unsigned char*>(out.data()));
		state.key = out;
		state.keyFor = id;
	}
	return state.key;
}

[[nodiscard]] QString Seal(const QJsonObject &json) {
	const auto plain = QJsonDocument(json).toJson(QJsonDocument::Compact);
	const auto key = DeriveKey();
	auto iv = QByteArray(12, Qt::Uninitialized);
	RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), 12);
	auto out = QByteArray(int(plain.size()) + 16, Qt::Uninitialized);
	auto len = 0;
	auto total = 0;
	const auto ctx = EVP_CIPHER_CTX_new();
	auto ok = ctx
		&& EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
		&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr)
		&& EVP_EncryptInit_ex(
			ctx,
			nullptr,
			nullptr,
			reinterpret_cast<const unsigned char*>(key.constData()),
			reinterpret_cast<const unsigned char*>(iv.constData()))
		&& EVP_EncryptUpdate(
			ctx,
			reinterpret_cast<unsigned char*>(out.data()),
			&len,
			reinterpret_cast<const unsigned char*>(plain.constData()),
			int(plain.size()));
	total = len;
	ok = ok && EVP_EncryptFinal_ex(
		ctx,
		reinterpret_cast<unsigned char*>(out.data()) + total,
		&len);
	total += len;
	auto tag = QByteArray(16, Qt::Uninitialized);
	ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());
	if (ctx) {
		EVP_CIPHER_CTX_free(ctx);
	}
	if (!ok) {
		return QString();
	}
	out.resize(total);
	const auto all = iv + out + tag;
	return U(kMark) + QString::fromLatin1(all.toBase64(
		QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

[[nodiscard]] QJsonObject Open(const QString &text) {
	if (!text.startsWith(U(kMark))) {
		return {};
	}
	const auto all = QByteArray::fromBase64(
		text.mid(U(kMark).size()).toLatin1(),
		QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	if (all.size() < 12 + 16) {
		return {};
	}
	const auto key = DeriveKey();
	const auto cipherSize = int(all.size()) - 12 - 16;
	auto tag = all.right(16);
	auto plain = QByteArray(cipherSize + 16, Qt::Uninitialized);
	auto len = 0;
	auto total = 0;
	const auto ctx = EVP_CIPHER_CTX_new();
	auto ok = ctx
		&& EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr)
		&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr)
		&& EVP_DecryptInit_ex(
			ctx,
			nullptr,
			nullptr,
			reinterpret_cast<const unsigned char*>(key.constData()),
			reinterpret_cast<const unsigned char*>(all.constData()))
		&& EVP_DecryptUpdate(
			ctx,
			reinterpret_cast<unsigned char*>(plain.data()),
			&len,
			reinterpret_cast<const unsigned char*>(all.constData()) + 12,
			cipherSize);
	total = len;
	ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag.data());
	ok = ok && EVP_DecryptFinal_ex(
		ctx,
		reinterpret_cast<unsigned char*>(plain.data()) + total,
		&len) > 0;
	if (ctx) {
		EVP_CIPHER_CTX_free(ctx);
	}
	if (!ok) {
		return {};
	}
	plain.resize(total + len);
	return QJsonDocument::fromJson(plain).object();
}

// --- отправка ---

void Send(QJsonObject json) {
	auto &state = Data();
	if (!state.session || !state.channel.bare) {
		return;
	}
	const auto channel = state.session->data().channelLoaded(state.channel);
	if (!channel) {
		return;
	}
	json.insert(u"d"_q, DeviceId());
	json.insert(u"ts"_q, double(QDateTime::currentMSecsSinceEpoch()));
	const auto text = Seal(json);
	if (text.isEmpty()) {
		return;
	}
	auto options = Api::SendOptions();
	options.silent = true;
	auto action = Api::SendAction(
		state.session->data().history(channel),
		options);
	auto message = Api::MessageToSend(std::move(action));
	message.textWithTags = TextWithTags{ text, {} };
	state.session->api().sendMessage(std::move(message));
}

void DeleteLater(MsgId id, crl::time delay) {
	auto &state = Data();
	const auto session = state.session;
	const auto channelId = state.channel;
	base::call_delayed(delay, session, [=] {
		const auto channel = session->data().channelLoaded(channelId);
		if (!channel) {
			return;
		}
		session->api().request(MTPchannels_DeleteMessages(
			channel->inputChannel(),
			MTP_vector<MTPint>(1, MTP_int(int(id.bare)))
		)).send();
	});
}

void Toast(const QString &text) {
	if (const auto window = Core::App().activePrimaryWindow()) {
		if (const auto controller = window->sessionController()) {
			controller->showToast(text);
		}
	}
}

[[nodiscard]] PeerId PeerFromAndroidDialog(qint64 dialog) {
	if (dialog > 0) {
		return peerFromUser(UserId(dialog));
	} else if (dialog <= -1000000000000LL) {
		return peerFromChannel(ChannelId(-dialog - 1000000000000LL));
	}
	return peerFromChat(ChatId(-dialog));
}

[[nodiscard]] qint64 AndroidDialogFromPeer(PeerId id) {
	if (peerIsUser(id)) {
		return qint64(peerToUser(id).bare);
	} else if (peerIsChannel(id)) {
		return -1000000000000LL - qint64(peerToChannel(id).bare);
	} else if (peerIsChat(id)) {
		return -qint64(peerToChat(id).bare);
	}
	return 0;
}

// --- заметки, теги, закрепы ---

void PublishState() {
	if (!Enabled() || !Flag("notes")) {
		return;
	}
	auto notes = QJsonObject();
	auto tags = QJsonObject();
	Notes::ForEach([&](PeerId id, const QString &tag, const QString &note) {
		if (!peerIsUser(id)) {
			return;
		}
		const auto key = QString::number(peerToUser(id).bare);
		if (!note.isEmpty()) {
			notes.insert(key, note);
		}
		if (!tag.isEmpty()) {
			tags.insert(key, tag);
		}
	});
	auto pins = QJsonArray();
	for (const auto &value : Tweaks::LocalPinList()) {
		const auto dialog = AndroidDialogFromPeer(PeerId(value.toULongLong()));
		if (dialog) {
			pins.push_back(QString::number(dialog));
		}
	}
	auto json = QJsonObject();
	json.insert(u"t"_q, u"state"_q);
	json.insert(u"notes"_q, notes);
	json.insert(u"tags"_q, tags);
	json.insert(u"pins"_q, pins);
	Settings::SetString(
		u"link.state_ts"_q,
		QString::number(QDateTime::currentMSecsSinceEpoch()));
	Send(json);
}

void ApplyState(const QJsonObject &json, qint64 ts) {
	const auto known = Settings::String(u"link.state_ts"_q).toLongLong();
	if (ts <= known) {
		return;
	}
	Settings::SetString(u"link.state_ts"_q, QString::number(ts));
	const auto notes = json.value(u"notes"_q).toObject();
	const auto tags = json.value(u"tags"_q).toObject();
	for (auto i = notes.begin(); i != notes.end(); ++i) {
		Notes::SetNote(peerFromUser(UserId(i.key().toULongLong())), i.value().toString());
	}
	for (auto i = tags.begin(); i != tags.end(); ++i) {
		Notes::SetTag(peerFromUser(UserId(i.key().toULongLong())), i.value().toString());
	}
	auto ids = QStringList();
	for (const auto &value : json.value(u"pins"_q).toArray()) {
		const auto peer = PeerFromAndroidDialog(value.toString().toLongLong());
		if (peer) {
			ids.push_back(QString::number(peer.value));
		}
	}
	if (const auto session = Data().session) {
		Tweaks::SetLocalPinList(session, ids);
	}
}

void SchedulePublishState() {
	auto &state = Data();
	if (!state.started) {
		return;
	}
	state.stateTimer.callOnce(3000);
}

// --- события ---

void Handle(const QString &text, MsgId id, crl::time now) {
	auto &state = Data();
	const auto json = Open(text);
	if (json.isEmpty()) {
		return;
	}
	const auto type = json.value(u"t"_q).toString();
	const auto device = json.value(u"d"_q).toString();
	if (device == DeviceId()) {
		if (type != u"state"_q) {
			DeleteLater(id, kEventTtl);
		}
		return;
	}
	const auto ts = qint64(json.value(u"ts"_q).toDouble());
	const auto fresh = (QDateTime::currentMSecsSinceEpoch() - ts) < kEventTtl;
	if (type == u"clip"_q) {
		if (fresh && Flag("clip")) {
			const auto value = json.value(u"text"_q).toString();
			if (!value.isEmpty()) {
				state.lastClipHash = QString::number(qHash(value));
				QGuiApplication::clipboard()->setText(value);
			}
		}
	} else if (type == u"now"_q) {
		if (fresh && Flag("music")) {
			const auto title = json.value(u"title"_q).toString();
			const auto artist = json.value(u"artist"_q).toString();
			if (!title.isEmpty()) {
				Toast(U(Mogram::Lang::Tr("Играет на телефоне: "))
					+ (artist.isEmpty() ? title : (artist + u" - "_q + title)));
			}
		}
	} else if (type == u"act"_q) {
		if (Flag("activity")) {
			state.remoteActive.clear();
			const auto chat = json.value(u"chat"_q).toVariant().toLongLong();
			if (chat && fresh) {
				state.remoteActive[PeerFromAndroidDialog(chat).value] = now;
			}
		}
	} else if (type == u"state"_q) {
		if (Flag("notes")) {
			ApplyState(json, ts);
		}
	}
}

void LoadHistory() {
	auto &state = Data();
	const auto session = state.session;
	const auto channel = session->data().channelLoaded(state.channel);
	if (!channel) {
		return;
	}
	session->api().request(MTPmessages_GetHistory(
		channel->input(),
		MTP_int(0),
		MTP_int(0),
		MTP_int(0),
		MTP_int(40),
		MTP_int(0),
		MTP_int(0),
		MTP_long(0)
	)).done([=](const MTPmessages_Messages &result) {
		const auto list = result.match([&](const MTPDmessages_messagesNotModified &) {
			return QVector<MTPMessage>();
		}, [&](const auto &data) {
			return data.vmessages().v;
		});
		for (auto i = list.size() - 1; i >= 0; --i) {
			list[i].match([&](const MTPDmessage &data) {
				Handle(qs(data.vmessage()), MsgId(data.vid().v), crl::now());
			}, [](const auto &) {});
		}
	}).send();
}

void ChannelReady() {
	auto &state = Data();
	const auto session = state.session;
	const auto channel = state.channel;
	state.lifetime.destroy();
	session->data().newItemAdded(
	) | rpl::on_next([=](not_null<HistoryItem*> item) {
		if (item->history()->peer->id != peerFromChannel(channel)) {
			return;
		}
		Handle(item->originalText().text, item->id, crl::now());
	}, state.lifetime);
	LoadHistory();
	PublishState();
}

void FindOrCreate() {
	auto &state = Data();
	const auto session = state.session;
	session->api().request(MTPcontacts_Search(
		MTP_flags(0),
		MTP_string(U(kChannelTitle)),
		MTP_int(20)
	)).done([=](const MTPcontacts_Found &result) {
		auto &state = Data();
		result.match([&](const MTPDcontacts_found &data) {
			session->data().processUsers(data.vusers());
			session->data().processChats(data.vchats());
			auto best = ChannelId();
			for (const auto &peer : data.vmy_results().v) {
				const auto id = peerFromMTP(peer);
				if (!peerIsChannel(id)) {
					continue;
				}
				const auto channel = session->data().channelLoaded(
					peerToChannel(id));
				if (channel
					&& channel->amCreator()
					&& !channel->isMegagroup()
					&& channel->name() == U(kChannelTitle)
					// Если каналов несколько, все устройства берут самый старый.
					&& (!best.bare || peerToChannel(id).bare < best.bare)) {
					best = peerToChannel(id);
				}
			}
			if (best.bare) {
				state.channel = best;
				ChannelReady();
			}
		});
		if (state.channel.bare || state.creating) {
			return;
		}
		state.creating = true;
		using Flag = MTPchannels_CreateChannel::Flag;
		session->api().request(MTPchannels_CreateChannel(
			MTP_flags(Flag::f_broadcast),
			MTP_string(U(kChannelTitle)),
			MTP_string(u"mgl1"_q),
			MTPInputGeoPoint(),
			MTPstring(),
			MTP_int(0)
		)).done([=](const MTPUpdates &updates) {
			auto &state = Data();
			state.creating = false;
			session->api().applyUpdates(updates);
			const auto chats = updates.match([&](const MTPDupdates &data) {
				return data.vchats().v;
			}, [&](const MTPDupdatesCombined &data) {
				return data.vchats().v;
			}, [](const auto &) {
				return QVector<MTPChat>();
			});
			for (const auto &chat : chats) {
				chat.match([&](const MTPDchannel &channel) {
					if (qs(channel.vtitle()) == U(kChannelTitle)) {
						state.channel = ChannelId(channel.vid().v);
					}
				}, [](const auto &) {});
			}
			if (state.channel.bare) {
				ChannelReady();
			}
		}).fail([=] {
			Data().creating = false;
		}).send();
	}).send();
}

void OnLocalClip() {
	auto &state = Data();
	if (!Enabled() || !Flag("clip") || !state.channel.bare) {
		return;
	}
	const auto text = QGuiApplication::clipboard()->text();
	if (text.isEmpty() || text.size() > 2000) {
		return;
	}
	const auto hash = QString::number(qHash(text));
	if (hash == state.lastClipHash) {
		return;
	}
	state.lastClipHash = hash;
	auto json = QJsonObject();
	json.insert(u"t"_q, u"clip"_q);
	json.insert(u"text"_q, text);
	Send(json);
}

void PublishNow() {
	auto &state = Data();
	if (!Enabled() || !Flag("music") || !state.channel.bare) {
		return;
	}
	auto title = QString();
	auto artist = QString();
	if (!Music::CurrentTrackInfo(title, artist) || title.isEmpty()) {
		return;
	}
	const auto key = artist + u"\n"_q + title;
	if (key == state.lastNowKey) {
		return;
	}
	state.lastNowKey = key;
	auto json = QJsonObject();
	json.insert(u"t"_q, u"now"_q);
	json.insert(u"title"_q, title);
	json.insert(u"artist"_q, artist);
	json.insert(u"sc"_q, 0);
	Send(json);
}

void Pulse() {
	// Раз в 5 секунд сообщаем, какой чат открыт, пока окно активно.
	auto &state = Data();
	if (!Enabled() || !Flag("activity") || !state.channel.bare) {
		return;
	}
	auto chat = qint64(0);
	if (const auto window = Core::App().activePrimaryWindow()) {
		const auto controller = window->sessionController();
		if (controller && window->widget()->isActiveWindow()) {
			if (const auto history = controller->activeChatCurrent().owningHistory()) {
				chat = AndroidDialogFromPeer(history->peer->id);
			}
		}
	}
	const auto now = crl::now();
	if (chat == state.pulseChat && (!chat || now - state.pulseAt < 15000)) {
		return;
	}
	state.pulseChat = chat;
	state.pulseAt = now;
	auto json = QJsonObject();
	json.insert(u"t"_q, u"act"_q);
	json.insert(u"chat"_q, QString::number(chat));
	Send(json);
}

void Attach(Main::Session *session) {
	auto &state = Data();
	state.lifetime.destroy();
	state.channel = ChannelId();
	state.creating = false;
	state.session = session;
	if (!session || !Enabled() || Password().isEmpty()) {
		return;
	}
	FindOrCreate();
}

void OpenPasswordBox() {
	const auto window = Core::App().activePrimaryWindow();
	const auto controller = window ? window->sessionController() : nullptr;
	if (!controller) {
		return;
	}
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(U(Mogram::Lang::Tr("Пароль связи"))));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			rpl::single(U(Mogram::Lang::Tr("Один на всех устройствах")))));
		box->setFocusCallback([=] {
			field->setFocusFast();
		});
		box->addButton(rpl::single(U(Mogram::Lang::Tr("Сохранить"))), [=] {
			const auto value = field->getLastText();
			const auto sealed = Secure::Seal(value.toUtf8()).toBase64();
			Settings::SetString(u"link.password"_q, QString::fromLatin1(sealed));
			Settings::SetString(
				u"link.password_label"_q,
				value.isEmpty()
					? U(Mogram::Lang::Tr("не задан"))
					: U(Mogram::Lang::Tr("задан")));
			box->closeBox();
			Attach(Data().session);
		});
		box->addButton(rpl::single(U(Mogram::Lang::Tr("Отмена"))), [=] {
			box->closeBox();
		});
	}));
}

} // namespace

bool ActiveElsewhere(quint64 peerValue) {
	if (!Flag("activity")) {
		return false;
	}
	const auto &map = Data().remoteActive;
	const auto i = map.find(peerValue);
	return (i != map.end()) && (crl::now() - i->second < 25000);
}

void RegisterSettings() {
	const auto section = U(Mogram::Lang::Tr("Mogram Link"));
	const auto add = [&](const char *key, const char *title, const char *description, bool value) {
		Settings::Register({
			.key = u"link."_q + QString::fromUtf8(key),
			.section = section,
			.title = U(title),
			.description = U(description),
			.kind = Settings::Kind::Bool,
			.defaultValue = value,
			.category = u"accounts"_q,
			.sectionIcon = u"devices"_q,
		});
	};
	Settings::Register({
		.key = u"link.enabled"_q,
		.section = section,
		.title = U(Mogram::Lang::Tr("Связь с телефоном")),
		.description = U(Mogram::Lang::Tr("Устройства общаются через закрытый канал «Mogram Link» в вашем аккаунте. Сообщения шифруются.")),
		.kind = Settings::Kind::Bool,
		.defaultValue = false,
		.category = u"accounts"_q,
		.sectionIcon = u"devices"_q,
	});
	Settings::Register({
		.key = u"link.password_label"_q,
		.section = section,
		.title = U(Mogram::Lang::Tr("Пароль связи")),
		.description = U(Mogram::Lang::Tr("На всех устройствах должен быть один пароль.")),
		.kind = Settings::Kind::String,
		.defaultValue = U(Mogram::Lang::Tr("не задан")),
		.category = u"accounts"_q,
		.sectionIcon = u"devices"_q,
		.action = [] { OpenPasswordBox(); },
	});
	add("clip", Mogram::Lang::Tr("Общий буфер обмена"), Mogram::Lang::Tr("Скопированный текст появляется в буфере на другом устройстве."), true);
	add("music", Mogram::Lang::Tr("Музыка между устройствами"), Mogram::Lang::Tr("Видно, что играет на телефоне."), true);
	add("notes", Mogram::Lang::Tr("Общие заметки, теги и закрепы"), Mogram::Lang::Tr("Заметки, теги и локальные закрепы одинаковы на ПК и телефоне."), true);
	add("activity", Mogram::Lang::Tr("Умный перехват активности"), Mogram::Lang::Tr("Нет уведомления, пока вы читаете этот чат на другом устройстве."), true);
	for (const auto key : { "link.password", "link.device", "link.state_ts" }) {
		Settings::Register({
			.key = QString::fromUtf8(key),
			.section = section,
			.title = QString::fromUtf8(key),
			.kind = Settings::Kind::String,
			.category = u"accounts"_q,
			.hidden = true,
		});
	}
}

void Start() {
	auto &state = Data();
	if (state.started) {
		return;
	}
	state.started = true;
	Core::App().domain().activeSessionValue(
	) | rpl::on_next([=](Main::Session *session) {
		Attach(session);
	}, state.appLifetime);
	Settings::Changed(
	) | rpl::filter([](const QString &key) {
		return (key == u"link.enabled"_q);
	}) | rpl::on_next([](const QString &) {
		Attach(Data().session);
	}, state.appLifetime);
	state.stateTimer.setCallback([] { PublishState(); });
	state.pulseTimer.setCallback([] { Pulse(); });
	state.pulseTimer.callEach(5000);
	Notes::Changed() | rpl::on_next([=](PeerId) {
		SchedulePublishState();
	}, state.appLifetime);
	QObject::connect(
		QGuiApplication::clipboard(),
		&QClipboard::dataChanged,
		[] { OnLocalClip(); });
	Media::Player::instance()->updatedNotifier(
	) | rpl::on_next([=](const Media::Player::TrackState &) {
		PublishNow();
	}, state.appLifetime);
}

} // namespace Mogram::Link
