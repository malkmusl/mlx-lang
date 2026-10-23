// The Mlx port's WebCrypto (MlxCryptoUtilities.h): the platform halves of
// the symmetric algorithms and key derivations (AES-CBC, AES-CFB-8,
// AES-CTR, AES-GCM, AES-KW, HMAC, HKDF, PBKDF2), what the other files
// share, and the serialized keys' wrapping.
#include "config.h"
#include "MlxCryptoUtilities.h"

#include "CryptoAlgorithmAESCBC.h"
#include "CryptoAlgorithmAESCFB.h"
#include "CryptoAlgorithmAESCTR.h"
#include "CryptoAlgorithmAESGCM.h"
#include "CryptoAlgorithmAESKW.h"
#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoAlgorithmAesCtrParams.h"
#include "CryptoAlgorithmAesGcmParams.h"
#include "CryptoAlgorithmHKDF.h"
#include "CryptoAlgorithmHMAC.h"
#include "CryptoAlgorithmHkdfParams.h"
#include "CryptoAlgorithmPBKDF2.h"
#include "CryptoAlgorithmPbkdf2Params.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyRaw.h"
#include "ExceptionOr.h"
#include "SerializedCryptoKeyWrap.h"
#include "WrappedCryptoKey.h"

namespace WebCore {

MlxCryptoKey::~MlxCryptoKey()
{
    mlxweb_crypto_rsa_release(rsa);
}

void MlxCryptoKeyDeleter::operator()(MlxCryptoKey* key) const
{
    delete key;
}

namespace MlxCrypto {

uint32_t hashIdentifier(CryptoAlgorithmIdentifier identifier)
{
    switch (identifier) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return MLXWEB_DIGEST_SHA1;
    case CryptoAlgorithmIdentifier::SHA_256:
        return MLXWEB_DIGEST_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return MLXWEB_DIGEST_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return MLXWEB_DIGEST_SHA512;
    default:
        return 0;
    }
}

Parts::Parts()
    : m_handle(mlxweb_crypto_parts_create())
{
}

Parts::~Parts()
{
    mlxweb_crypto_parts_release(m_handle);
}

size_t Parts::size() const
{
    return mlxweb_crypto_parts_count(m_handle);
}

std::span<const uint8_t> Parts::at(size_t index) const
{
    size_t length = 0;
    auto* bytes = mlxweb_crypto_parts_at(m_handle, index, &length);
    if (!bytes || !length)
        return { };
    return unsafeMakeSpan(bytes, length);
}

void Parts::add(std::span<const uint8_t> bytes)
{
    mlxweb_crypto_parts_add(m_handle, bytes.data(), bytes.size());
}

} // namespace MlxCrypto

static Exception operationError()
{
    return Exception { ExceptionCode::OperationError };
}

// AES-CBC

static ExceptionOr<Vector<uint8_t>> aesCbc(bool encrypt, const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& data, CryptoAlgorithmAESCBC::Padding padding)
{
    auto& iv = parameters.ivVector();
    if (iv.size() != 16)
        return operationError();
    Vector<uint8_t> output(data.size() + 16);
    size_t written = 0;
    if (!mlxweb_crypto_aes_cbc(encrypt, padding == CryptoAlgorithmAESCBC::Padding::Yes, key.key().span().data(), key.key().size(), iv.span().data(), data.span().data(), data.size(), output.mutableSpan().data(), &written))
        return operationError();
    output.shrink(written);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCBC::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText, Padding padding)
{
    return aesCbc(true, parameters, key, plainText, padding);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCBC::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText, Padding padding)
{
    return aesCbc(false, parameters, key, cipherText, padding);
}

// AES-CFB-8

static ExceptionOr<Vector<uint8_t>> aesCfb(bool encrypt, const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    auto& iv = parameters.ivVector();
    if (iv.size() != 16)
        return operationError();
    Vector<uint8_t> output(data.size());
    if (!mlxweb_crypto_aes_cfb8(encrypt, key.key().span().data(), key.key().size(), iv.span().data(), data.span().data(), data.size(), output.mutableSpan().data()))
        return operationError();
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCFB::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    return aesCfb(true, parameters, key, plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCFB::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    return aesCfb(false, parameters, key, cipherText);
}

// AES-CTR (the same both ways)

static ExceptionOr<Vector<uint8_t>> aesCtr(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    auto& counter = parameters.counterVector();
    if (counter.size() != 16 || !parameters.length || parameters.length > 128)
        return operationError();
    // (More blocks than the counter's bits count: a counter used twice.)
    size_t blocks = (data.size() + 15) / 16;
    if (parameters.length < 64 && blocks > (uint64_t(1) << parameters.length))
        return operationError();
    Vector<uint8_t> output(data.size());
    if (!mlxweb_crypto_aes_ctr(key.key().span().data(), key.key().size(), counter.span().data(), parameters.length, data.span().data(), data.size(), output.mutableSpan().data()))
        return operationError();
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCTR::platformEncrypt(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    return aesCtr(parameters, key, plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCTR::platformDecrypt(const CryptoAlgorithmAesCtrParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    return aesCtr(parameters, key, cipherText);
}

// AES-GCM (the tag after the ciphertext)

static ExceptionOr<Vector<uint8_t>> aesGcm(bool encrypt, const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    size_t tagLength = parameters.tagLength.value_or(128) / 8;
    auto& iv = parameters.ivVector();
    auto& aad = parameters.additionalDataVector();
    if (!encrypt && data.size() < tagLength)
        return operationError();
    Vector<uint8_t> output(encrypt ? data.size() + tagLength : data.size() - tagLength);
    if (!mlxweb_crypto_aes_gcm(encrypt, key.key().span().data(), key.key().size(), iv.span().data(), iv.size(), aad.span().data(), aad.size(), tagLength, data.span().data(), data.size(), output.mutableSpan().data()))
        return operationError();
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformEncrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText)
{
    return aesGcm(true, parameters, key, plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformDecrypt(const CryptoAlgorithmAesGcmParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText)
{
    return aesGcm(false, parameters, key, cipherText);
}

// AES-KW

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformWrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    if (data.size() < 16 || data.size() % 8)
        return operationError();
    Vector<uint8_t> output(data.size() + 8);
    if (!mlxweb_crypto_aes_kw(true, key.key().span().data(), key.key().size(), data.span().data(), data.size(), output.mutableSpan().data()))
        return operationError();
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformUnwrapKey(const CryptoKeyAES& key, const Vector<uint8_t>& data)
{
    if (data.size() < 24 || data.size() % 8)
        return operationError();
    Vector<uint8_t> output(data.size() - 8);
    if (!mlxweb_crypto_aes_kw(false, key.key().span().data(), key.key().size(), data.span().data(), data.size(), output.mutableSpan().data()))
        return operationError();
    return output;
}

// HMAC

static std::optional<Vector<uint8_t>> hmac(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto hash = MlxCrypto::hashIdentifier(key.hashAlgorithmIdentifier());
    if (!hash)
        return std::nullopt;
    Vector<uint8_t> output(64);
    size_t length = mlxweb_crypto_hmac(hash, key.key().span().data(), key.key().size(), data.span().data(), data.size(), output.mutableSpan().data());
    if (!length)
        return std::nullopt;
    output.shrink(length);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHMAC::platformSign(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto result = hmac(key, data);
    if (!result)
        return operationError();
    return WTF::move(*result);
}

ExceptionOr<bool> CryptoAlgorithmHMAC::platformVerify(const CryptoKeyHMAC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto result = hmac(key, data);
    if (!result)
        return operationError();
    if (result->size() != signature.size())
        return false;
    uint8_t difference = 0;
    for (size_t i = 0; i < signature.size(); ++i)
        difference |= (*result)[i] ^ signature[i];
    return !difference;
}

// HKDF and PBKDF2 (LENGTH in bits, a multiple of 8: CryptoAlgorithm*'s
// deriveBits checks it)

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams& parameters, const CryptoKeyRaw& key, size_t length)
{
    auto hash = MlxCrypto::hashIdentifier(parameters.hashIdentifier);
    if (!hash)
        return operationError();
    auto& salt = parameters.saltVector();
    auto& info = parameters.infoVector();
    Vector<uint8_t> output(length / 8);
    if (!mlxweb_crypto_hkdf(hash, key.key().span().data(), key.key().size(), salt.span().data(), salt.size(), info.span().data(), info.size(), output.mutableSpan().data(), output.size()))
        return operationError();
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmPBKDF2::platformDeriveBits(const CryptoAlgorithmPbkdf2Params& parameters, const CryptoKeyRaw& key, size_t length)
{
    auto hash = MlxCrypto::hashIdentifier(parameters.hashIdentifier);
    if (!hash || !parameters.iterations)
        return operationError();
    auto& salt = parameters.saltVector();
    Vector<uint8_t> output(length / 8);
    if (!mlxweb_crypto_pbkdf2(hash, key.key().span().data(), key.key().size(), salt.span().data(), salt.size(), parameters.iterations, output.mutableSpan().data(), output.size()))
        return operationError();
    return output;
}

// Keys kept in IndexedDB and history: as crypto/gcrypt has it, the
// serialized key is kept as it is (no master key wraps it: WebKit's
// bug 173883).

std::optional<Vector<uint8_t>> defaultWebCryptoMasterKey()
{
    return Vector<uint8_t>(FillWith { }, 16, 0);
}

bool wrapSerializedCryptoKey(const Vector<uint8_t>&, const Vector<uint8_t>& key, Vector<uint8_t>& result)
{
    result = key;
    return true;
}

std::optional<struct WrappedCryptoKey> readSerializedCryptoKey(const Vector<uint8_t>& wrappedKey)
{
    std::array<uint8_t, 24> a { 0 };
    std::array<uint8_t, 16> b { 0 };
    return WrappedCryptoKey { a, wrappedKey, b };
}

std::optional<Vector<uint8_t>> unwrapCryptoKey(const Vector<uint8_t>&, const struct WrappedCryptoKey& wrappedKey)
{
    return wrappedKey.encryptedKey;
}

} // namespace WebCore
