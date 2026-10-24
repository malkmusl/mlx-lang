// window.crypto for the Mlx port (PORT.md, "Crypto"): WebKit's
// page/Crypto.cpp with getRandomValues() and randomUUID() taking the
// kernel's random bytes through std.random (the port's crypto.mlx), where
// WebKit's reads WTF's ARC4 stream (CommonCrypto on Darwin).
// lists/webcore-mlx-skipped.txt leaves WebKit's file out.
#include "config.h"
#include "Crypto.h"

#include "Document.h"
#include "ExceptionOr.h"
#include "SubtleCrypto.h"
#include "port/MlxWebPort.h"
#include <JavaScriptCore/ArrayBufferView.h>
#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Crypto::Crypto(ScriptExecutionContext* context)
    : ContextDestructionObserver(context)
    , m_subtle(SubtleCrypto::create(context))
{
}

Crypto::~Crypto() = default;

ExceptionOr<void> Crypto::getRandomValues(ArrayBufferView& array)
{
    if (!isInt(array.getType()) && !isBigInt(array.getType()))
        return Exception { ExceptionCode::TypeMismatchError };
    if (array.byteLength() > 65536)
        return Exception { ExceptionCode::QuotaExceededError };
    auto bytes = array.mutableSpan();
    RELEASE_ASSERT(mlxweb_crypto_random(bytes.data(), bytes.size()));
    return { };
}

// A version 4 UUID (RFC 9562, 5.4): 122 random bits, the version's and
// the variant's set, written 8-4-4-4-12.
String Crypto::randomUUID() const
{
    std::array<uint8_t, 16> bytes;
    RELEASE_ASSERT(mlxweb_crypto_random(bytes.data(), bytes.size()));
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    StringBuilder builder;
    for (size_t index = 0; index < bytes.size(); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10)
            builder.append('-');
        builder.append(hex(bytes[index], 2, Lowercase));
    }
    return builder.toString();
}

} // namespace WebCore
