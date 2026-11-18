// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// mogram-id: 981700e5
#pragma once

#include <QtCore/QByteArray>

namespace Mogram::Update {

// WHY: AppVersion stays the upstream Telegram Desktop version, so releases
// of Mogram are ordered by their own number, read by publish_update.sh.
constexpr auto kVersion = 2;
constexpr auto kVersionTitle = "1.1";

// Ed25519-подпись ключом Mogram. Используется и магазином плагинов.
[[nodiscard]] bool VerifyDetached(
	const QByteArray &message,
	const QByteArray &signature);

void RegisterSettings();
void Start();
void CheckNow();

} // namespace Mogram::Update
