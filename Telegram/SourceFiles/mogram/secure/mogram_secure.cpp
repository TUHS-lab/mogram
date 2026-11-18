// This file is part of Mogram, https://github.com/TUHS-lab/mogram
// (c) 2026 @tuhss (https://t.me/tuhss). GPLv3 with the attribution term
// of MOGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
// Mogram: шифрование секретов на диске.
#include "mogram/secure/mogram_secure.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")
#endif // Q_OS_WIN

namespace Mogram::Secure {
namespace {

// Метка в начале файла: отличает зашифрованные данные от старых открытых.
constexpr auto kMagic = "MGS1";

#ifdef Q_OS_WIN
// Дополнительная энтропия привязывает данные к Mogram: другая программа
// того же пользователя не расшифрует их простым вызовом DPAPI.
[[nodiscard]] DATA_BLOB Entropy() {
	static const auto bytes = QByteArray("mogram.secure.v1");
	return DATA_BLOB{
		DWORD(bytes.size()),
		reinterpret_cast<BYTE*>(const_cast<char*>(bytes.constData())),
	};
}
#endif // Q_OS_WIN

} // namespace

QByteArray Seal(const QByteArray &plain) {
#ifdef Q_OS_WIN
	if (plain.isEmpty()) {
		return plain;
	}
	auto in = DATA_BLOB{
		DWORD(plain.size()),
		reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData())),
	};
	auto entropy = Entropy();
	auto out = DATA_BLOB();
	if (!CryptProtectData(
			&in,
			L"Mogram",
			&entropy,
			nullptr,
			nullptr,
			CRYPTPROTECT_UI_FORBIDDEN,
			&out)) {
		return plain;
	}
	auto result = QByteArray(kMagic)
		+ QByteArray(reinterpret_cast<const char*>(out.pbData), int(out.cbData));
	LocalFree(out.pbData);
	return result;
#else // Q_OS_WIN
	return plain;
#endif // Q_OS_WIN
}

QByteArray Open(const QByteArray &stored) {
	if (!stored.startsWith(kMagic)) {
		return stored;
	}
#ifdef Q_OS_WIN
	const auto body = stored.mid(int(qstrlen(kMagic)));
	auto in = DATA_BLOB{
		DWORD(body.size()),
		reinterpret_cast<BYTE*>(const_cast<char*>(body.constData())),
	};
	auto entropy = Entropy();
	auto out = DATA_BLOB();
	if (!CryptUnprotectData(
			&in,
			nullptr,
			&entropy,
			nullptr,
			nullptr,
			CRYPTPROTECT_UI_FORBIDDEN,
			&out)) {
		return QByteArray();
	}
	auto result = QByteArray(
		reinterpret_cast<const char*>(out.pbData),
		int(out.cbData));
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
	return result;
#else // Q_OS_WIN
	return QByteArray();
#endif // Q_OS_WIN
}

} // namespace Mogram::Secure
