// The Mlx port's WebCrypto (MlxCryptoUtilities.h): EC keys on P-256,
// P-384 and P-521, ECDSA and ECDH (std.crypto.ecc).
#include "config.h"
#include "MlxCryptoUtilities.h"

#include "CryptoAlgorithmECDH.h"
#include "CryptoAlgorithmECDSA.h"
#include "CryptoAlgorithmEcdsaParams.h"
#include "CryptoKeyEC.h"
#include "ExceptionOr.h"
#include "JsonWebKey.h"
#include <wtf/text/Base64.h>

namespace WebCore {

static uint32_t curveBits(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    case CryptoKeyEC::NamedCurve::P521:
        return 521;
    }
    return 0;
}

static size_t curveBytes(CryptoKeyEC::NamedCurve curve)
{
    return mlxweb_crypto_ec_size(curveBits(curve));
}

// A key of the curve from a point (any SEC 1 form, checked to be on the
// curve) and, for a private key, a scalar whose public point it is.
static RefPtr<CryptoKeyEC> makeKey(CryptoAlgorithmIdentifier identifier, CryptoKeyEC::NamedCurve curve, std::span<const uint8_t> point, std::span<const uint8_t> scalar, bool isPrivate, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto bits = curveBits(curve);
    size_t size = curveBytes(curve);
    auto key = std::unique_ptr<MlxCryptoKey, MlxCryptoKeyDeleter>(new MlxCryptoKey);
    key->curve = bits;
    key->point.grow(1 + 2 * size);
    if (isPrivate) {
        if (scalar.size() != size)
            return nullptr;
        key->scalar = Vector<uint8_t>(scalar);
        Vector<uint8_t> derived(1 + 2 * size);
        if (!mlxweb_crypto_ec_public(bits, scalar.data(), derived.mutableSpan().data()))
            return nullptr;
        if (point.empty())
            key->point = WTF::move(derived);
        else {
            if (!mlxweb_crypto_ec_point(bits, point.data(), point.size(), key->point.mutableSpan().data()) || key->point != derived)
                return nullptr;
        }
    } else if (point.empty() || !mlxweb_crypto_ec_point(bits, point.data(), point.size(), key->point.mutableSpan().data()))
        return nullptr;
    return CryptoKeyEC::create(identifier, curve, isPrivate ? CryptoKeyType::Private : CryptoKeyType::Public, WTF::move(key), extractable, usages);
}

bool CryptoKeyEC::platformSupportedCurve(NamedCurve curve)
{
    return curveBits(curve);
}

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    size_t size = curveBytes(curve);
    Vector<uint8_t> scalar(size);
    Vector<uint8_t> point(1 + 2 * size);
    if (!mlxweb_crypto_ec_generate(curveBits(curve), scalar.mutableSpan().data(), point.mutableSpan().data()))
        return std::nullopt;
    auto publicKey = makeKey(identifier, curve, point.span(), { }, false, true, usages);
    auto privateKey = makeKey(identifier, curve, point.span(), scalar.span(), true, extractable, usages);
    if (!publicKey || !privateKey)
        return std::nullopt;
    return CryptoKeyPair { WTF::move(publicKey), WTF::move(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    return makeKey(identifier, curve, keyData.span(), { }, false, extractable, usages);
}

static Vector<uint8_t> uncompressed(const Vector<uint8_t>& x, const Vector<uint8_t>& y)
{
    Vector<uint8_t> point;
    point.append(4);
    point.appendVector(x);
    point.appendVector(y);
    return point;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (x.size() != curveBytes(curve) || y.size() != curveBytes(curve))
        return nullptr;
    return makeKey(identifier, curve, uncompressed(x, y).span(), { }, false, extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (x.size() != curveBytes(curve) || y.size() != curveBytes(curve))
        return nullptr;
    return makeKey(identifier, curve, uncompressed(x, y).span(), d.span(), true, extractable, usages);
}

// The parts read hold an EC key of the curve the algorithm may take (an
// id-ecDH key for ECDH alone).
static bool readKindFits(const MlxCrypto::Parts& parts, CryptoAlgorithmIdentifier identifier, uint32_t bits)
{
    auto header = parts.at(0);
    if (header.size() != 3)
        return false;
    uint32_t kind = header[0];
    if (kind != MLXWEB_CRYPTO_KIND_EC && !(kind == MLXWEB_CRYPTO_KIND_ECDH && identifier == CryptoAlgorithmIdentifier::ECDH))
        return false;
    return ((header[1] << 8) | header[2]) == bits;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    MlxCrypto::Parts parts(mlxweb_crypto_spki_read(keyData.span().data(), keyData.size()));
    if (!parts || parts.size() != 2 || !readKindFits(parts, identifier, curveBits(curve)))
        return nullptr;
    return makeKey(identifier, curve, parts.at(1), { }, false, extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    MlxCrypto::Parts parts(mlxweb_crypto_pkcs8_read(keyData.span().data(), keyData.size()));
    if (!parts || parts.size() != 3 || !readKindFits(parts, identifier, curveBits(curve)))
        return nullptr;
    return makeKey(identifier, curve, parts.at(2), parts.at(1), true, extractable, usages);
}

size_t CryptoKeyEC::keySizeInBits() const
{
    return curveBits(m_curve);
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    return m_platformKey->point;
}

bool CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    size_t size = curveBytes(m_curve);
    auto& point = m_platformKey->point;
    if (point.size() != 1 + 2 * size)
        return false;
    jwk.x = base64URLEncodeToString(point.span().subspan(1, size));
    jwk.y = base64URLEncodeToString(point.span().subspan(1 + size, size));
    if (type() == Type::Private)
        jwk.d = base64URLEncodeToString(m_platformKey->scalar);
    return true;
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    MlxCrypto::Parts key;
    key.add(m_platformKey->point.span());
    MlxCrypto::Parts written(mlxweb_crypto_spki_write(MLXWEB_CRYPTO_KIND_EC, curveBits(m_curve), key.handle()));
    if (!written || written.size() != 1)
        return { };
    return written.vectorAt(0);
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    MlxCrypto::Parts key;
    key.add(m_platformKey->scalar.span());
    key.add(m_platformKey->point.span());
    MlxCrypto::Parts written(mlxweb_crypto_pkcs8_write(MLXWEB_CRYPTO_KIND_EC, curveBits(m_curve), key.handle()));
    if (!written || written.size() != 1)
        return { };
    return written.vectorAt(0);
}

// ECDSA: r | s, each of the curve's bytes.

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& data)
{
    auto hash = MlxCrypto::hashIdentifier(parameters.hashIdentifier);
    auto& platformKey = *key.platformKey();
    if (!hash || platformKey.scalar.isEmpty())
        return Exception { ExceptionCode::OperationError };
    Vector<uint8_t> signature(2 * mlxweb_crypto_ec_size(platformKey.curve));
    if (!mlxweb_crypto_ecdsa_sign(platformKey.curve, hash, platformKey.scalar.span().data(), data.span().data(), data.size(), signature.mutableSpan().data()))
        return Exception { ExceptionCode::OperationError };
    return signature;
}

ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams& parameters, const CryptoKeyEC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto hash = MlxCrypto::hashIdentifier(parameters.hashIdentifier);
    if (!hash)
        return Exception { ExceptionCode::OperationError };
    auto& platformKey = *key.platformKey();
    return !!mlxweb_crypto_ecdsa_verify(platformKey.curve, hash, platformKey.point.span().data(), platformKey.point.size(), data.span().data(), data.size(), signature.span().data(), signature.size());
}

// ECDH: the shared x coordinate.

std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC& baseKey, const CryptoKeyEC& publicKey)
{
    auto& base = *baseKey.platformKey();
    auto& peer = *publicKey.platformKey();
    if (base.scalar.isEmpty() || base.curve != peer.curve)
        return std::nullopt;
    Vector<uint8_t> secret(mlxweb_crypto_ec_size(base.curve));
    if (!mlxweb_crypto_ecdh(base.curve, base.scalar.span().data(), peer.point.span().data(), peer.point.size(), secret.mutableSpan().data()))
        return std::nullopt;
    return secret;
}

} // namespace WebCore
