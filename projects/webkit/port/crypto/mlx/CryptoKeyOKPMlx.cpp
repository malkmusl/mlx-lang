// The Mlx port's WebCrypto (MlxCryptoUtilities.h): Ed25519 and X25519
// keys and algorithms (std.crypto.curve25519).
//
// WebKit's CryptoKeyOKP.cpp, CryptoAlgorithmEd25519.cpp and
// CryptoAlgorithmX25519.cpp define these platform functions themselves,
// unsupported, for a port that is neither Cocoa nor GCrypt (`!PLATFORM(COCOA)
// && !USE(GCRYPT)`): this unit, compiled apart (SourcesMlx.txt's
// @no-unify), compiles those three files with USE(GCRYPT)'s selection, so
// that their own code is WebKit's and the platform half is the port's
// (lists/webcore-mlx-skipped.txt leaves them out of WebCore's bundles).
// Nothing they include depends on USE(GCRYPT).
#include "config.h"

#undef USE_GCRYPT
#define USE_GCRYPT 1
#include "crypto/keys/CryptoKeyOKP.cpp"
#include "crypto/algorithms/CryptoAlgorithmEd25519.cpp"
#include "crypto/algorithms/CryptoAlgorithmX25519.cpp"
#undef USE_GCRYPT
#define USE_GCRYPT 0

#include "MlxCryptoUtilities.h"

namespace WebCore {

static constexpr size_t keyBytes = 32;

// The public key of a private one (an Ed25519 seed, an X25519 scalar).
static std::optional<Vector<uint8_t>> publicKeyOf(CryptoKeyOKP::NamedCurve curve, const Vector<uint8_t>& privateKey)
{
    if (privateKey.size() != keyBytes)
        return std::nullopt;
    Vector<uint8_t> publicKey(keyBytes);
    if (curve == CryptoKeyOKP::NamedCurve::Ed25519)
        mlxweb_crypto_ed25519_public(privateKey.span().data(), publicKey.mutableSpan().data());
    else if (!mlxweb_crypto_x25519(privateKey.span().data(), nullptr, publicKey.mutableSpan().data()))
        return std::nullopt;
    return publicKey;
}

bool CryptoKeyOKP::supportsNamedCurve()
{
    return true;
}

std::optional<CryptoKeyPair> CryptoKeyOKP::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve namedCurve, bool extractable, CryptoKeyUsageBitmap usages)
{
    Vector<uint8_t> privateKeyData(keyBytes);
    if (!mlxweb_crypto_random(privateKeyData.mutableSpan().data(), keyBytes))
        return std::nullopt;
    auto publicKeyData = publicKeyOf(namedCurve, privateKeyData);
    if (!publicKeyData)
        return std::nullopt;
    auto publicKey = CryptoKeyOKP::create(identifier, namedCurve, CryptoKeyType::Public, WTF::move(*publicKeyData), true, usages);
    auto privateKey = CryptoKeyOKP::create(identifier, namedCurve, CryptoKeyType::Private, WTF::move(privateKeyData), extractable, usages);
    if (!publicKey || !privateKey)
        return std::nullopt;
    return CryptoKeyPair { WTF::move(publicKey), WTF::move(privateKey) };
}

bool CryptoKeyOKP::platformCheckPairedKeys(CryptoAlgorithmIdentifier, NamedCurve namedCurve, const Vector<uint8_t>& privateKey, const Vector<uint8_t>& publicKey)
{
    auto derived = publicKeyOf(namedCurve, privateKey);
    return derived && *derived == publicKey;
}

static uint32_t kindOf(CryptoKeyOKP::NamedCurve curve)
{
    return curve == CryptoKeyOKP::NamedCurve::Ed25519 ? MLXWEB_CRYPTO_KIND_ED25519 : MLXWEB_CRYPTO_KIND_X25519;
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    MlxCrypto::Parts parts(mlxweb_crypto_spki_read(keyData.span().data(), keyData.size()));
    if (!parts || parts.size() != 2 || parts.at(0)[0] != kindOf(curve) || parts.at(1).size() != keyBytes)
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Public, parts.vectorAt(1), extractable, usages);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { ExceptionCode::InvalidAccessError };
    MlxCrypto::Parts key;
    key.add(m_data.span());
    MlxCrypto::Parts written(mlxweb_crypto_spki_write(kindOf(m_curve), 0, key.handle()));
    if (!written || written.size() != 1)
        return Exception { ExceptionCode::OperationError };
    return written.vectorAt(0);
}

RefPtr<CryptoKeyOKP> CryptoKeyOKP::importPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    MlxCrypto::Parts parts(mlxweb_crypto_pkcs8_read(keyData.span().data(), keyData.size()));
    if (!parts || parts.size() != 2 || parts.at(0)[0] != kindOf(curve) || parts.at(1).size() != keyBytes)
        return nullptr;
    return create(identifier, curve, CryptoKeyType::Private, parts.vectorAt(1), extractable, usages);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { ExceptionCode::InvalidAccessError };
    MlxCrypto::Parts key;
    key.add(m_data.span());
    MlxCrypto::Parts written(mlxweb_crypto_pkcs8_write(kindOf(m_curve), 0, key.handle()));
    if (!written || written.size() != 1)
        return Exception { ExceptionCode::OperationError };
    return written.vectorAt(0);
}

String CryptoKeyOKP::generateJwkD() const
{
    ASSERT(type() == CryptoKeyType::Private);
    return base64URLEncodeToString(m_data);
}

String CryptoKeyOKP::generateJwkX() const
{
    if (type() == CryptoKeyType::Public)
        return base64URLEncodeToString(m_data);
    auto publicKey = publicKeyOf(m_curve, m_data);
    return publicKey ? base64URLEncodeToString(*publicKey) : String();
}

Vector<uint8_t> CryptoKeyOKP::platformExportRaw() const
{
    return m_data;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmEd25519::platformSign(const CryptoKeyOKP& key, const Vector<uint8_t>& data)
{
    if (key.type() != CryptoKeyType::Private || key.platformKey().size() != keyBytes)
        return Exception { ExceptionCode::OperationError };
    Vector<uint8_t> signature(64);
    mlxweb_crypto_ed25519_sign(key.platformKey().span().data(), data.span().data(), data.size(), signature.mutableSpan().data());
    return signature;
}

ExceptionOr<bool> CryptoAlgorithmEd25519::platformVerify(const CryptoKeyOKP& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    if (key.platformKey().size() != keyBytes)
        return Exception { ExceptionCode::OperationError };
    return !!mlxweb_crypto_ed25519_verify(key.platformKey().span().data(), data.span().data(), data.size(), signature.span().data(), signature.size());
}

std::optional<Vector<uint8_t>> CryptoAlgorithmX25519::platformDeriveBits(const CryptoKeyOKP& baseKey, const CryptoKeyOKP& publicKey)
{
    if (baseKey.platformKey().size() != keyBytes || publicKey.platformKey().size() != keyBytes)
        return std::nullopt;
    Vector<uint8_t> secret(keyBytes);
    if (!mlxweb_crypto_x25519(baseKey.platformKey().span().data(), publicKey.platformKey().span().data(), secret.mutableSpan().data()))
        return std::nullopt;
    return secret;
}

} // namespace WebCore
