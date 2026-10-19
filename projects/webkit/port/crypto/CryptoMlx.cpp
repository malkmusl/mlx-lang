// The Mlx port's WebCrypto platform functions (PORT.md, "Compression,
// WOFF, crypto"): what crypto/gcrypt and crypto/openssl define for their
// ports. WebCrypto is always built in 2.54 (no ENABLE_WEB_CRYPTO), so the
// port defines them all now.
//
// STUB (M7: crypto/mlx over std.crypto, std.aes, std.asn1, std.x509):
// only the digests are registered, which WebCore's own CryptoAlgorithmSHA*
// compute with PAL's CryptoDigest (std.sha1, std.sha2: CryptoDigestMlx.cpp).
// The other algorithms are not registered, so crypto.subtle rejects them
// as not supported before any function below is reached; the functions
// fail the same way, and no key is ever made (MlxPrefix.h's MlxCryptoKey
// is never allocated).
#include "config.h"

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
#include "CryptoAlgorithmSHA1.h"
#include "CryptoAlgorithmSHA256.h"
#include "CryptoAlgorithmSHA384.h"
#include "CryptoAlgorithmSHA512.h"
#include "CryptoAlgorithmX25519.h"
#include "CryptoKeyEC.h"
#include "CryptoKeyOKP.h"
#include "CryptoKeyRSA.h"
#include "CryptoKeyRSAComponents.h"
#include "ExceptionOr.h"
#include "JsonWebKey.h"
#include "SerializedCryptoKeyWrap.h"

namespace WebCore {

void MlxCryptoKeyDeleter::operator()(MlxCryptoKey*) const
{
}

void CryptoAlgorithmRegistry::platformRegisterAlgorithms()
{
    registerAlgorithm<CryptoAlgorithmSHA1>();
    registerAlgorithm<CryptoAlgorithmSHA256>();
    registerAlgorithm<CryptoAlgorithmSHA384>();
    registerAlgorithm<CryptoAlgorithmSHA512>();
}

static Exception notSupported()
{
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCBC::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams&, const CryptoKeyAES&, const Vector<uint8_t>&, Padding) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCBC::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams&, const CryptoKeyAES&, const Vector<uint8_t>&, Padding) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCFB::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCFB::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCTR::platformEncrypt(const CryptoAlgorithmAesCtrParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCTR::platformDecrypt(const CryptoAlgorithmAesCtrParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformEncrypt(const CryptoAlgorithmAesGcmParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESGCM::platformDecrypt(const CryptoAlgorithmAesGcmParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformWrapKey(const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESKW::platformUnwrapKey(const CryptoKeyAES&, const Vector<uint8_t>&) { return notSupported(); }
std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC&, const CryptoKeyEC&) { return std::nullopt; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams&, const CryptoKeyEC&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams&, const CryptoKeyEC&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmEd25519::platformSign(const CryptoKeyOKP&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<bool> CryptoAlgorithmEd25519::platformVerify(const CryptoKeyOKP&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHKDF::platformDeriveBits(const CryptoAlgorithmHkdfParams&, const CryptoKeyRaw&, size_t) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHMAC::platformSign(const CryptoKeyHMAC&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<bool> CryptoAlgorithmHMAC::platformVerify(const CryptoKeyHMAC&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmPBKDF2::platformDeriveBits(const CryptoAlgorithmPbkdf2Params&, const CryptoKeyRaw&, size_t) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams&, const CryptoKeyRSA&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams&, const CryptoKeyRSA&, const Vector<uint8_t>&) { return notSupported(); }
#if HAVE(RSA_PSS)
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_PSS::platformSign(const CryptoAlgorithmRsaPssParams&, const CryptoKeyRSA&, const Vector<uint8_t>&) { return notSupported(); }
ExceptionOr<bool> CryptoAlgorithmRSA_PSS::platformVerify(const CryptoAlgorithmRsaPssParams&, const CryptoKeyRSA&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return notSupported(); }
#endif
std::optional<Vector<uint8_t>> CryptoAlgorithmX25519::platformDeriveBits(const CryptoKeyOKP&, const CryptoKeyOKP&) { return std::nullopt; }

// Keys: none is made.

bool CryptoKeyEC::platformSupportedCurve(NamedCurve) { return false; }
std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier, NamedCurve, bool, CryptoKeyUsageBitmap) { return std::nullopt; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, Vector<uint8_t>&&, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
size_t CryptoKeyEC::keySizeInBits() const { return 0; }
Vector<uint8_t> CryptoKeyEC::platformExportRaw() const { return { }; }
bool CryptoKeyEC::platformAddFieldElements(JsonWebKey&) const { return false; }
Vector<uint8_t> CryptoKeyEC::platformExportSpki() const { return { }; }
Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const { return { }; }

bool CryptoKeyOKP::supportsNamedCurve() { return false; }
std::optional<CryptoKeyPair> CryptoKeyOKP::platformGeneratePair(CryptoAlgorithmIdentifier, NamedCurve, bool, CryptoKeyUsageBitmap) { return std::nullopt; }
bool CryptoKeyOKP::platformCheckPairedKeys(CryptoAlgorithmIdentifier, NamedCurve, const Vector<uint8_t>&, const Vector<uint8_t>&) { return false; }
RefPtr<CryptoKeyOKP> CryptoKeyOKP::importSpki(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyOKP> CryptoKeyOKP::importPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportSpki() const { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoKeyOKP::exportPkcs8() const { return notSupported(); }
String CryptoKeyOKP::generateJwkD() const { return { }; }
String CryptoKeyOKP::generateJwkX() const { return { }; }
Vector<uint8_t> CryptoKeyOKP::platformExportRaw() const { return { }; }

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& platformKey, bool extractable, CryptoKeyUsageBitmap usage)
    : CryptoKey(identifier, type, extractable, usage)
    , m_platformKey(WTF::move(platformKey))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier, bool, const CryptoKeyRSAComponents&, bool, CryptoKeyUsageBitmap) { return nullptr; }

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier& identifier) const
{
    if (!m_restrictedToSpecificHash)
        return false;
    identifier = m_hash;
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const { return 0; }

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier, bool, unsigned, const Vector<uint8_t>&, bool, CryptoKeyUsageBitmap, KeyPairCallback&&, VoidCallback&& failureCallback, ScriptExecutionContext*)
{
    failureCallback();
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const { return notSupported(); }
ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const { return notSupported(); }

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    return CryptoKeyAlgorithm { CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier()) };
}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const { return nullptr; }

// Keys kept in IndexedDB and history are wrapped with a master key of the
// user's (the keyring's, on the ports with one): none here yet.
std::optional<Vector<uint8_t>> defaultWebCryptoMasterKey() { return std::nullopt; }
bool wrapSerializedCryptoKey(const Vector<uint8_t>&, const Vector<uint8_t>&, Vector<uint8_t>&) { return false; }
std::optional<struct WrappedCryptoKey> readSerializedCryptoKey(const Vector<uint8_t>&) { return std::nullopt; }
std::optional<Vector<uint8_t>> unwrapCryptoKey(const Vector<uint8_t>&, const struct WrappedCryptoKey&) { return std::nullopt; }

} // namespace WebCore
