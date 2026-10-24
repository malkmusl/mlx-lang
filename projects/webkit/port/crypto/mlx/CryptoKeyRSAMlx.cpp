// The Mlx port's WebCrypto (MlxCryptoUtilities.h): RSA keys,
// RSASSA-PKCS1-v1_5, RSA-PSS and RSA-OAEP (std.crypto.rsa, keys made with
// std.crypto.mpi), and the registry of the algorithms the port has.
//
// RSA-PSS is compiled under HAVE(RSA_PSS), which WTF's Platform.h sets
// for the ports of a named crypto library only (its FIXME: "should not
// depend on the policy decision to USE(GCRYPT)"): this unit, compiled
// apart (SourcesMlx.txt's @no-unify), has it, and compiles WebKit's
// CryptoAlgorithmRSA_PSS.cpp with it (its unified bundle's copy is empty).
#include "config.h"

#undef HAVE_RSA_PSS
#define HAVE_RSA_PSS 1

#include "MlxCryptoUtilities.h"

#include "CryptoAlgorithmAESCBC.h"
#include "CryptoAlgorithmAESCFB.h"
#include "CryptoAlgorithmAESCTR.h"
#include "CryptoAlgorithmAESGCM.h"
#include "CryptoAlgorithmAESKW.h"
#include "CryptoAlgorithmECDH.h"
#include "CryptoAlgorithmECDSA.h"
#include "CryptoAlgorithmEd25519.h"
#include "CryptoAlgorithmHKDF.h"
#include "CryptoAlgorithmHMAC.h"
#include "CryptoAlgorithmPBKDF2.h"
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"
#include "CryptoAlgorithmRSA_OAEP.h"
#include "CryptoAlgorithmRSA_PSS.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoAlgorithmRsaPssParams.h"
#include "CryptoAlgorithmSHA1.h"
#include "CryptoAlgorithmSHA256.h"
#include "CryptoAlgorithmSHA384.h"
#include "CryptoAlgorithmSHA512.h"
#include "CryptoAlgorithmX25519.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSA.h"
#include "CryptoKeyRSAComponents.h"
#include "ExceptionOr.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WorkQueue.h>

#include "crypto/algorithms/CryptoAlgorithmRSA_PSS.cpp"

namespace WebCore {

void CryptoAlgorithmRegistry::platformRegisterAlgorithms()
{
    // (AES-CFB-8 is deprecated in WebKit and registered by none of its
    // Linux ports: its platform functions are defined, it is not offered.)
    registerAlgorithm<CryptoAlgorithmAESCBC>();
    registerAlgorithm<CryptoAlgorithmAESCTR>();
    registerAlgorithm<CryptoAlgorithmAESGCM>();
    registerAlgorithm<CryptoAlgorithmAESKW>();
    registerAlgorithm<CryptoAlgorithmECDH>();
    registerAlgorithm<CryptoAlgorithmECDSA>();
    registerAlgorithm<CryptoAlgorithmEd25519>();
    registerAlgorithm<CryptoAlgorithmHKDF>();
    registerAlgorithm<CryptoAlgorithmHMAC>();
    registerAlgorithm<CryptoAlgorithmPBKDF2>();
    registerAlgorithm<CryptoAlgorithmRSASSA_PKCS1_v1_5>();
    registerAlgorithm<CryptoAlgorithmRSA_OAEP>();
    registerAlgorithm<CryptoAlgorithmRSA_PSS>();
    registerAlgorithm<CryptoAlgorithmSHA1>();
    registerAlgorithm<CryptoAlgorithmSHA256>();
    registerAlgorithm<CryptoAlgorithmSHA384>();
    registerAlgorithm<CryptoAlgorithmSHA512>();
    registerAlgorithm<CryptoAlgorithmX25519>();
}

enum RSANumber : size_t { Modulus, Exponent, PrivateExponent, FirstPrime, SecondPrime, FirstExponent, SecondExponent, Coefficient, NumberCount };
// Each prime after the second (a multi-prime key, RFC 8017 3.2): its
// r, d and t after the eight.
constexpr size_t OtherPrimeNumbers = 3;

static bool isPrivate(const Vector<Vector<uint8_t>>& numbers)
{
    return numbers.size() >= NumberCount && !((numbers.size() - NumberCount) % OtherPrimeNumbers);
}

// The platform key of NUMBERS (n and e, or the eight and the others'
// three each; the primes and the CRT's values may be empty), with the
// Mlx side's key made of them (a multi-prime one works with d alone, its
// primes' product checked).
static PlatformRSAKeyContainer makeKey(Vector<Vector<uint8_t>>&& numbers)
{
    if (numbers.size() != 2 && !isPrivate(numbers))
        return nullptr;
    MlxCrypto::Parts parts;
    parts.add(numbers[Modulus].span());
    parts.add(numbers[Exponent].span());
    if (isPrivate(numbers)) {
        parts.add(numbers[PrivateExponent].span());
        parts.add(numbers[FirstPrime].span());
        parts.add(numbers[SecondPrime].span());
        for (size_t index = NumberCount; index < numbers.size(); index += OtherPrimeNumbers)
            parts.add(numbers[index].span());
    }
    auto rsa = mlxweb_crypto_rsa_key(parts.handle());
    if (!rsa)
        return nullptr;
    PlatformRSAKeyContainer key(new MlxCryptoKey);
    key->numbers = WTF::move(numbers);
    key->rsa = rsa;
    return key;
}

static CryptoKeyType typeOf(const Vector<Vector<uint8_t>>& numbers)
{
    return isPrivate(numbers) ? CryptoKeyType::Private : CryptoKeyType::Public;
}

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(WTF::move(platformKey))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    Vector<Vector<uint8_t>> numbers;
    numbers.append(keyData.modulus());
    numbers.append(keyData.exponent());
    if (keyData.type() == CryptoKeyRSAComponents::Type::Private) {
        numbers.append(keyData.privateExponent());
        numbers.append(keyData.firstPrimeInfo().primeFactor);
        numbers.append(keyData.secondPrimeInfo().primeFactor);
        numbers.append(keyData.firstPrimeInfo().factorCRTExponent);
        numbers.append(keyData.secondPrimeInfo().factorCRTExponent);
        numbers.append(keyData.secondPrimeInfo().factorCRTCoefficient);
        for (auto& info : keyData.otherPrimeInfos()) {
            if (info.primeFactor.isEmpty())
                return nullptr;
            numbers.append(info.primeFactor);
            numbers.append(info.factorCRTExponent);
            numbers.append(info.factorCRTCoefficient);
        }
    }
    auto type = typeOf(numbers);
    auto key = makeKey(WTF::move(numbers));
    if (!key)
        return nullptr;
    return adoptRef(new CryptoKeyRSA(identifier, hash, hasHash, type, WTF::move(key), extractable, usages));
}

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier& identifier) const
{
    if (!m_restrictedToSpecificHash)
        return false;
    identifier = m_hash;
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const
{
    return mlxweb_crypto_rsa_bits(m_platformKey->rsa);
}

// The keys of a pair the Mlx side made (parts n, e, d, p, q, dp, dq, qi).
static std::optional<CryptoKeyPair> pairOf(MlxCrypto::Parts& made, CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (!made || made.size() != NumberCount)
        return std::nullopt;
    Vector<Vector<uint8_t>> publicNumbers;
    publicNumbers.append(made.vectorAt(Modulus));
    publicNumbers.append(made.vectorAt(Exponent));
    Vector<Vector<uint8_t>> privateNumbers;
    for (size_t index = 0; index < NumberCount; ++index)
        privateNumbers.append(made.vectorAt(index));
    auto publicPlatformKey = makeKey(WTF::move(publicNumbers));
    auto privatePlatformKey = makeKey(WTF::move(privateNumbers));
    if (!publicPlatformKey || !privatePlatformKey)
        return std::nullopt;
    auto publicKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Public, WTF::move(publicPlatformKey), true, usages);
    auto privateKey = CryptoKeyRSA::create(algorithm, hash, hasHash, CryptoKeyType::Private, WTF::move(privatePlatformKey), extractable, usages);
    return CryptoKeyPair { WTF::move(publicKey), WTF::move(privateKey) };
}

// A pair's callbacks, kept for the context's thread while the numbers
// are made.
struct KeyPairCallbacks {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(KeyPairCallbacks);
    CryptoKeyRSA::KeyPairCallback success;
    CryptoKeyRSA::VoidCallback failure;
};

// The queue keys are made on: the primes of a 4096-bit key take seconds,
// of a 16384-bit one minutes, which the page's thread does not wait for.
static WorkQueue& keyGenerationQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("org.mlx.WebKit.RSAKeyGeneration"_s));
    return queue.get();
}

// The numbers are made on keyGenerationQueue and the keys from them on
// the context's thread, where the callbacks are called (as WebKit's
// Cocoa port does: a pair made on a global queue, handed back with
// ScriptExecutionContext::postTaskTo). Without a context, made at once.
void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier algorithm, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& publicExponent, bool extractable, CryptoKeyUsageBitmap usages, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext* context)
{
    // (The exponent's leading zeroes off: [0, 1, 0, 1] is 65537.)
    size_t start = 0;
    while (start < publicExponent.size() && !publicExponent[start])
        ++start;
    Vector<uint8_t> exponent(publicExponent.span().subspan(start));
    if (!context) {
        MlxCrypto::Parts made(mlxweb_crypto_rsa_generate(modulusLength, exponent.span().data(), exponent.size()));
        auto pair = pairOf(made, algorithm, hash, hasHash, extractable, usages);
        if (!pair) {
            failureCallback();
            return;
        }
        callback(WTF::move(*pair));
        return;
    }
    // (The callbacks hold the page's objects: they never leave its
    // thread. Should the context be gone when the numbers are, they are
    // left, as Cocoa's are, not destroyed on the queue.)
    auto callbacks = makeUnique<KeyPairCallbacks>(KeyPairCallbacks { WTF::move(callback), WTF::move(failureCallback) });
    keyGenerationQueue().dispatch([identifier = context->identifier(), algorithm, hash, hasHash, modulusLength, exponent = WTF::move(exponent), extractable, usages, callbacks = callbacks.release()]() mutable {
        uintptr_t made = mlxweb_crypto_rsa_generate(modulusLength, exponent.span().data(), exponent.size());
        bool posted = ScriptExecutionContext::postTaskTo(identifier, [made, algorithm, hash, hasHash, extractable, usages, callbacks](ScriptExecutionContext&) {
            std::unique_ptr<KeyPairCallbacks> owned(callbacks);
            MlxCrypto::Parts parts(made);
            auto pair = pairOf(parts, algorithm, hash, hasHash, extractable, usages);
            if (!pair) {
                owned->failure();
                return;
            }
            owned->success(WTF::move(*pair));
        });
        if (!posted)
            MlxCrypto::Parts released(made);
    });
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    MlxCrypto::Parts parts(mlxweb_crypto_spki_read(keyData.span().data(), keyData.size()));
    if (!parts || parts.size() != 3 || parts.at(0)[0] != MLXWEB_CRYPTO_KIND_RSA)
        return nullptr;
    Vector<Vector<uint8_t>> numbers;
    numbers.append(parts.vectorAt(1));
    numbers.append(parts.vectorAt(2));
    auto key = makeKey(WTF::move(numbers));
    if (!key)
        return nullptr;
    return adoptRef(new CryptoKeyRSA(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Public, WTF::move(key), extractable, usages));
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    MlxCrypto::Parts parts(mlxweb_crypto_pkcs8_read(keyData.span().data(), keyData.size()));
    if (!parts || parts.size() < 1 + NumberCount || (parts.size() - 1 - NumberCount) % OtherPrimeNumbers || parts.at(0)[0] != MLXWEB_CRYPTO_KIND_RSA)
        return nullptr;
    Vector<Vector<uint8_t>> numbers;
    for (size_t index = 1; index < parts.size(); ++index)
        numbers.append(parts.vectorAt(index));
    auto key = makeKey(WTF::move(numbers));
    if (!key)
        return nullptr;
    return adoptRef(new CryptoKeyRSA(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), !!hash, CryptoKeyType::Private, WTF::move(key), extractable, usages));
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { ExceptionCode::InvalidAccessError };
    MlxCrypto::Parts key;
    key.add(m_platformKey->numbers[Modulus].span());
    key.add(m_platformKey->numbers[Exponent].span());
    MlxCrypto::Parts written(mlxweb_crypto_spki_write(MLXWEB_CRYPTO_KIND_RSA, 0, key.handle()));
    if (!written || written.size() != 1)
        return Exception { ExceptionCode::OperationError };
    return written.vectorAt(0);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { ExceptionCode::InvalidAccessError };
    auto& numbers = m_platformKey->numbers;
    MlxCrypto::Parts key;
    for (auto& number : numbers) {
        if (number.isEmpty())
            return Exception { ExceptionCode::OperationError };
        key.add(number.span());
    }
    MlxCrypto::Parts written(mlxweb_crypto_pkcs8_write(MLXWEB_CRYPTO_KIND_RSA, 0, key.handle()));
    if (!written || written.size() != 1)
        return Exception { ExceptionCode::OperationError };
    return written.vectorAt(0);
}

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    auto modulusLength = static_cast<unsigned>(keySizeInBits());
    auto& publicExponent = m_platformKey->numbers[Exponent];
    if (m_restrictedToSpecificHash) {
        return CryptoRsaHashedKeyAlgorithm {
            CryptoRsaKeyAlgorithm {
                CryptoKeyAlgorithm { CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier()) },
                modulusLength,
                Uint8Array::create(publicExponent.span())
            },
            CryptoKeyAlgorithm { CryptoAlgorithmRegistry::singleton().name(m_hash) }
        };
    }
    return CryptoRsaKeyAlgorithm {
        CryptoKeyAlgorithm { CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier()) },
        modulusLength,
        Uint8Array::create(publicExponent.span())
    };
}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const
{
    auto& numbers = m_platformKey->numbers;
    if (type() == CryptoKeyType::Public)
        return CryptoKeyRSAComponents::createPublic(numbers[Modulus], numbers[Exponent]);
    if (numbers[FirstPrime].isEmpty())
        return CryptoKeyRSAComponents::createPrivate(numbers[Modulus], numbers[Exponent], numbers[PrivateExponent]);
    CryptoKeyRSAComponents::PrimeInfo firstPrimeInfo;
    firstPrimeInfo.primeFactor = numbers[FirstPrime];
    firstPrimeInfo.factorCRTExponent = numbers[FirstExponent];
    CryptoKeyRSAComponents::PrimeInfo secondPrimeInfo;
    secondPrimeInfo.primeFactor = numbers[SecondPrime];
    secondPrimeInfo.factorCRTExponent = numbers[SecondExponent];
    secondPrimeInfo.factorCRTCoefficient = numbers[Coefficient];
    Vector<CryptoKeyRSAComponents::PrimeInfo> otherPrimeInfos;
    for (size_t index = NumberCount; index + OtherPrimeNumbers <= numbers.size(); index += OtherPrimeNumbers) {
        CryptoKeyRSAComponents::PrimeInfo info;
        info.primeFactor = numbers[index];
        info.factorCRTExponent = numbers[index + 1];
        info.factorCRTCoefficient = numbers[index + 2];
        otherPrimeInfos.append(WTF::move(info));
    }
    return CryptoKeyRSAComponents::createPrivateWithAdditionalData(numbers[Modulus], numbers[Exponent], numbers[PrivateExponent], WTF::move(firstPrimeInfo), WTF::move(secondPrimeInfo), WTF::move(otherPrimeInfos));
}

// Signatures and OAEP, with the key's hash.

static ExceptionOr<Vector<uint8_t>> rsaSign(const CryptoKeyRSA& key, uint32_t scheme, size_t saltLength, const Vector<uint8_t>& data)
{
    auto hash = MlxCrypto::hashIdentifier(key.hashAlgorithmIdentifier());
    if (!hash || key.type() != CryptoKeyType::Private)
        return Exception { ExceptionCode::OperationError };
    Vector<uint8_t> signature((key.keySizeInBits() + 7) / 8);
    size_t length = mlxweb_crypto_rsa_sign(key.platformKey()->rsa, scheme, hash, saltLength, data.span().data(), data.size(), signature.mutableSpan().data());
    if (!length)
        return Exception { ExceptionCode::OperationError };
    signature.shrink(length);
    return signature;
}

static ExceptionOr<bool> rsaVerify(const CryptoKeyRSA& key, uint32_t scheme, size_t saltLength, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto hash = MlxCrypto::hashIdentifier(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { ExceptionCode::OperationError };
    return !!mlxweb_crypto_rsa_verify(key.platformKey()->rsa, scheme, hash, saltLength, data.span().data(), data.size(), signature.span().data(), signature.size());
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return rsaSign(key, MLXWEB_CRYPTO_RSA_PKCS1, 0, data);
}

ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return rsaVerify(key, MLXWEB_CRYPTO_RSA_PKCS1, 0, signature, data);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_PSS::platformSign(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return rsaSign(key, MLXWEB_CRYPTO_RSA_PSS, parameters.saltLength, data);
}

ExceptionOr<bool> CryptoAlgorithmRSA_PSS::platformVerify(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return rsaVerify(key, MLXWEB_CRYPTO_RSA_PSS, parameters.saltLength, signature, data);
}

static ExceptionOr<Vector<uint8_t>> rsaOaep(bool encrypt, const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    auto hash = MlxCrypto::hashIdentifier(key.hashAlgorithmIdentifier());
    if (!hash)
        return Exception { ExceptionCode::OperationError };
    auto& label = parameters.labelVector();
    Vector<uint8_t> output((key.keySizeInBits() + 7) / 8);
    size_t written = 0;
    if (!mlxweb_crypto_rsa_oaep(encrypt, key.platformKey()->rsa, hash, label.span().data(), label.size(), data.span().data(), data.size(), output.mutableSpan().data(), &written))
        return Exception { ExceptionCode::OperationError };
    output.shrink(written);
    return output;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    return rsaOaep(true, parameters, key, plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    return rsaOaep(false, parameters, key, cipherText);
}

} // namespace WebCore
