#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

class QIODevice;

namespace Mogram::MessageInfo {

[[nodiscard]] QString VoicePlatform(const QByteArray &prefix);
[[nodiscard]] QString RoundVideoPlatform(QIODevice &file);

} // namespace Mogram::MessageInfo
