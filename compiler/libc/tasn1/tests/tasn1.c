/* mlx-tasn1 as WebCore's PAL::TASN1 and WebCrypto drive it: the
   definitions of WebCrypto.asn (the array asn1Parser makes, as
   Source/WebCore/PAL/pal/crypto/tasn1/Utilities.cpp has it) by
   asn1_array2tree; SubjectPublicKeyInfo, PrivateKeyInfo, ECParameters,
   ECPrivateKey, RSAPublicKey, RSAPrivateKey and CurvePrivateKey decoded
   from OpenSSL's keys and their elements read (asn1_read_value_type
   first with no room, as elementData does); the same structures written
   element by element and encoded, as the key exports do; and the errors
   of malformed DER, missing elements and values. It prints what
   libtasn1 4.19 prints for the same program. */
#include <libtasn1.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tasn1_vectors.h"

static const asn1_static_node definitions_array[] = {
    { "WebCrypto", 536872976, NULL },
    { NULL, 1073741836, NULL },
    { "SubjectPublicKeyInfo", 1610612741, NULL },
    { "algorithm", 1073741826, "AlgorithmIdentifier" },
    { "subjectPublicKey", 6, NULL },
    { "AlgorithmIdentifier", 1610612741, NULL },
    { "algorithm", 1073741836, NULL },
    { "parameters", 541081613, NULL },
    { "algorithm", 1, NULL },
    { "PrivateKeyInfo", 1610612741, NULL },
    { "version", 1073741826, "Version" },
    { "privateKeyAlgorithm", 1073741826, "PrivateKeyAlgorithmIdentifier" },
    { "privateKey", 1073741826, "PrivateKey" },
    { "attributes", 536895490, "Attributes" },
    { NULL, 4104, "0" },
    { "Version", 1073741827, NULL },
    { "PrivateKeyAlgorithmIdentifier", 1073741826, "AlgorithmIdentifier" },
    { "PrivateKey", 1073741831, NULL },
    { "CurvePrivateKey", 1073741831, NULL },
    { "Attributes", 1610612751, NULL },
    { NULL, 2, "Attribute" },
    { "Attribute", 1610612741, NULL },
    { "type", 1073741836, NULL },
    { "values", 2, "AttributeSetValue" },
    { "AttributeSetValue", 1610612751, NULL },
    { NULL, 13, NULL },
    { "ECParameters", 1610612754, NULL },
    { "namedCurve", 12, NULL },
    { "ECPrivateKey", 1610612741, NULL },
    { "version", 1073741827, NULL },
    { "privateKey", 1073741831, NULL },
    { "parameters", 1610637314, "ECParameters" },
    { NULL, 2056, "0" },
    { "publicKey", 536895494, NULL },
    { NULL, 2056, "1" },
    { "RSAPublicKey", 1610612741, NULL },
    { "modulus", 1073741827, NULL },
    { "publicExponent", 3, NULL },
    { "RSAPrivateKey", 1610612741, NULL },
    { "version", 1073741826, "Version" },
    { "modulus", 1073741827, NULL },
    { "publicExponent", 1073741827, NULL },
    { "privateExponent", 1073741827, NULL },
    { "prime1", 1073741827, NULL },
    { "prime2", 1073741827, NULL },
    { "exponent1", 1073741827, NULL },
    { "exponent2", 1073741827, NULL },
    { "coefficient", 1073741827, NULL },
    { "otherPrimeInfos", 16386, "OtherPrimeInfos" },
    { "OtherPrimeInfos", 1612709899, NULL },
    { "MAX", 1074266122, "1" },
    { NULL, 2, "OtherPrimeInfo" },
    { "OtherPrimeInfo", 536870917, NULL },
    { "prime", 1073741827, NULL },
    { "exponent", 1073741827, NULL },
    { "coefficient", 3, NULL },
    { NULL, 0, NULL }
};

static asn1_node definitions;

static size_t unhex(const char *text, unsigned char **out)
{
    size_t length = strlen(text) / 2;
    *out = malloc(length + 1);
    for (size_t i = 0; i < length; i++) {
        unsigned int byte;
        sscanf(text + 2 * i, "%2x", &byte);
        (*out)[i] = (unsigned char)byte;
    }
    return length;
}

static void hex(const char *label, const unsigned char *data, int length)
{
    printf("%s [%d]:", label, length);
    for (int i = 0; i < length && i < 300; i++)
        printf("%02x", data[i]);
    if (length > 300)
        printf("...");
    printf("\n");
}

/* As PAL::TASN1::elementData reads an element. */
static void element(asn1_node root, const char *name)
{
    int length = 0;
    unsigned int type = 0;
    int ret = asn1_read_value_type(root, name, NULL, &length, &type);
    printf("  %s: first %d (%s), length %d, type %u\n", name, ret, asn1_strerror(ret), length, type);
    if (ret != ASN1_MEM_ERROR)
        return;
    int bytes = type == ASN1_ETYPE_BIT_STRING ? (length + 7) / 8 : length;
    unsigned char *data = malloc(bytes + 1);
    int size = bytes;
    ret = asn1_read_value(root, name, data, &size);
    printf("    read %d, size %d\n", ret, size);
    if (ret == ASN1_SUCCESS) {
        if (type == ASN1_ETYPE_OBJECT_ID || type == ASN1_ETYPE_CHOICE)
            printf("    text \"%s\"\n", data);
        else
            hex("    data", data, bytes);
    }
    /* One byte short. */
    if (bytes > 1) {
        size = bytes - 1;
        ret = asn1_read_value(root, name, data, &size);
        printf("    short read %d, size %d\n", ret, size);
    }
    free(data);
}

static asn1_node decode(const char *label, const char *type, const unsigned char *der, int length)
{
    asn1_node root = NULL;
    int ret = asn1_create_element(definitions, type, &root);
    printf("%s: create %s %d\n", label, type, ret);
    int size = length;
    char error[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = "";
    ret = asn1_der_decoding2(&root, der, &size, ASN1_DECODE_FLAG_STRICT_DER, error);
    printf("  decode %d (%s), size %d, root %s\n", ret, asn1_strerror(ret), size, root ? "set" : "NULL");
    return root;
}

static int encode(const char *label, asn1_node root, const char *name, unsigned char **out)
{
    int length = 0;
    char error[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = "";
    int ret = asn1_der_coding(root, name, NULL, &length, error);
    printf("%s: size %d (%s), length %d\n", label, ret, asn1_strerror(ret), length);
    *out = NULL;
    if (ret != ASN1_MEM_ERROR)
        return 0;
    *out = malloc(length);
    ret = asn1_der_coding(root, name, *out, &length, error);
    printf("  code %d, length %d\n", ret, length);
    if (ret == ASN1_SUCCESS)
        hex("  der", *out, length);
    int shorter = length - 1;
    unsigned char *small = malloc(length);
    ret = asn1_der_coding(root, name, small, &shorter, error);
    printf("  one short %d, length %d\n", ret, shorter);
    free(small);
    return length;
}

static void write_value(asn1_node root, const char *name, const void *value, int length)
{
    int ret = asn1_write_value(root, name, value, length);
    printf("  write %s: %d (%s)\n", name, ret, asn1_strerror(ret));
}

static void decoding(void)
{
    struct { const char *label, *hex; } spkis[] = { { "ec spki", EC_SPKI }, { "rsa spki", RSA_SPKI }, { "ed25519 spki", ED25519_SPKI }, { "x25519 spki", X25519_SPKI } };
    for (int i = 0; i < 4; i++) {
        unsigned char *der;
        size_t length = unhex(spkis[i].hex, &der);
        asn1_node spki = decode(spkis[i].label, "WebCrypto.SubjectPublicKeyInfo", der, length);
        element(spki, "algorithm.algorithm");
        element(spki, "algorithm.parameters");
        element(spki, "subjectPublicKey");
        element(spki, "algorithm");
        element(spki, "");
        element(spki, "nothing");
        if (i == 0) {
            int size = 64;
            unsigned char parameters[64];
            asn1_read_value(spki, "algorithm.parameters", parameters, &size);
            asn1_node ec = decode("  ec parameters", "WebCrypto.ECParameters", parameters, size);
            element(ec, "namedCurve");
            element(ec, "");
            asn1_delete_structure(&ec);
        }
        if (i == 1) {
            int size = 512;
            unsigned char key[512];
            asn1_read_value(spki, "subjectPublicKey", key, &size);
            asn1_node rsa = decode("  rsa public key", "WebCrypto.RSAPublicKey", key, size / 8);
            element(rsa, "modulus");
            element(rsa, "publicExponent");
            asn1_delete_structure(&rsa);
        }
        asn1_delete_structure(&spki);
        printf("  deleted %s\n", spki ? "set" : "NULL");
        free(der);
    }

    struct { const char *label, *hex; } pkcs8s[] = { { "ec pkcs8", EC_PKCS8 }, { "rsa pkcs8", RSA_PKCS8 }, { "ed25519 pkcs8", ED25519_PKCS8 }, { "x25519 pkcs8", X25519_PKCS8 } };
    for (int i = 0; i < 4; i++) {
        unsigned char *der;
        size_t length = unhex(pkcs8s[i].hex, &der);
        asn1_node pkcs8 = decode(pkcs8s[i].label, "WebCrypto.PrivateKeyInfo", der, length);
        element(pkcs8, "version");
        element(pkcs8, "privateKeyAlgorithm.algorithm");
        element(pkcs8, "privateKeyAlgorithm.parameters");
        element(pkcs8, "privateKey");
        element(pkcs8, "attributes");
        int size = 2048;
        unsigned char inner[2048];
        asn1_read_value(pkcs8, "privateKey", inner, &size);
        if (i == 0) {
            asn1_node ec = decode("  ec private key", "WebCrypto.ECPrivateKey", inner, size);
            element(ec, "version");
            element(ec, "privateKey");
            element(ec, "parameters");
            element(ec, "parameters.namedCurve");
            element(ec, "publicKey");
            asn1_delete_structure(&ec);
        } else if (i == 1) {
            asn1_node rsa = decode("  rsa private key", "WebCrypto.RSAPrivateKey", inner, size);
            const char *names[] = { "version", "modulus", "publicExponent", "privateExponent", "prime1", "prime2", "exponent1", "exponent2", "coefficient", "otherPrimeInfos" };
            for (int n = 0; n < 10; n++)
                element(rsa, names[n]);
            asn1_delete_structure(&rsa);
        } else {
            asn1_node curve = decode("  curve private key", "WebCrypto.CurvePrivateKey", inner, size);
            element(curve, "");
            asn1_delete_structure(&curve);
        }
        asn1_delete_structure(&pkcs8);
        free(der);
    }

    /* The keys' own forms. */
    unsigned char *der;
    size_t length = unhex(EC_TRADITIONAL, &der);
    asn1_node ec = decode("ec traditional", "WebCrypto.ECPrivateKey", der, length);
    element(ec, "parameters.namedCurve");
    element(ec, "publicKey");
    asn1_delete_structure(&ec);
    free(der);
    length = unhex(RSA_TRADITIONAL, &der);
    asn1_node rsa = decode("rsa traditional", "WebCrypto.RSAPrivateKey", der, length);
    element(rsa, "coefficient");
    asn1_delete_structure(&rsa);

    /* Malformed. */
    asn1_node bad = decode("truncated", "WebCrypto.RSAPrivateKey", der, length - 1);
    asn1_delete_structure(&bad);
    unsigned char *longer = malloc(length + 2);
    memcpy(longer, der, length);
    longer[length] = 0;
    longer[length + 1] = 0;
    bad = decode("trailing bytes", "WebCrypto.RSAPrivateKey", longer, length + 2);
    asn1_delete_structure(&bad);
    bad = decode("wrong structure", "WebCrypto.SubjectPublicKeyInfo", der, length);
    asn1_delete_structure(&bad);
    unsigned char garbage[] = { 0x30, 0x05, 0x02, 0x01, 0x01, 0x04, 0x00 };
    bad = decode("garbage", "WebCrypto.RSAPublicKey", garbage, sizeof garbage);
    asn1_delete_structure(&bad);
    unsigned char nonminimal[] = { 0x30, 0x81, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x03 };
    bad = decode("non-minimal length", "WebCrypto.RSAPublicKey", nonminimal, sizeof nonminimal);
    asn1_delete_structure(&bad);
    unsigned char minimal[] = { 0x30, 0x06, 0x02, 0x01, 0x01, 0x02, 0x01, 0x03 };
    bad = decode("minimal", "WebCrypto.RSAPublicKey", minimal, sizeof minimal);
    element(bad, "modulus");
    element(bad, "publicExponent");
    asn1_delete_structure(&bad);
    free(longer);
    free(der);
    asn1_node none = NULL;
    printf("create unknown: %d\n", asn1_create_element(definitions, "WebCrypto.Nothing", &none));
}

static void encoding(void)
{
    /* CryptoKeyEC::platformExportSpki. */
    const unsigned char ecPublicKeyIdentifier[] = "1.2.840.10045.2.1";
    const unsigned char secp256r1[] = "1.2.840.10045.3.1.7";
    unsigned char *point;
    size_t pointLength = unhex(EC_SPKI, &point);
    asn1_node spki = NULL, ecParameters = NULL;
    printf("create spki %d\n", asn1_create_element(definitions, "WebCrypto.SubjectPublicKeyInfo", &spki));
    write_value(spki, "algorithm.algorithm", ecPublicKeyIdentifier, 1);
    printf("create ec parameters %d\n", asn1_create_element(definitions, "WebCrypto.ECParameters", &ecParameters));
    write_value(ecParameters, "", "namedCurve", 1);
    write_value(ecParameters, "namedCurve", secp256r1, 1);
    unsigned char *parameters;
    int parametersLength = encode("ec parameters", ecParameters, "", &parameters);
    write_value(spki, "algorithm.parameters", parameters, parametersLength);
    /* The point is the SPKI's last 65 bytes. */
    write_value(spki, "subjectPublicKey", point + pointLength - 65, 65 * 8);
    unsigned char *spkiDer;
    int spkiLength = encode("ec spki", spki, "", &spkiDer);
    printf("  same as OpenSSL's %d\n", spkiLength == (int)pointLength && memcmp(spkiDer, point, pointLength) == 0);
    element(spki, "algorithm.algorithm");
    unsigned char *part;
    encode("ec spki's algorithm", spki, "algorithm", &part);
    free(part);
    free(spkiDer);

    /* CryptoKeyEC::platformExportPkcs8. */
    unsigned char *keyDer;
    size_t keyLength = unhex(EC_TRADITIONAL, &keyDer);
    asn1_node original = NULL;
    asn1_create_element(definitions, "WebCrypto.ECPrivateKey", &original);
    int size = keyLength;
    asn1_der_decoding2(&original, keyDer, &size, 0, NULL);
    unsigned char d[32];
    size = 32;
    asn1_read_value(original, "privateKey", d, &size);
    asn1_node ecPrivateKey = NULL, pkcs8 = NULL;
    printf("create ec private key %d\n", asn1_create_element(definitions, "WebCrypto.ECPrivateKey", &ecPrivateKey));
    write_value(ecPrivateKey, "version", "1", 0);
    write_value(ecPrivateKey, "privateKey", d, 32);
    write_value(ecPrivateKey, "parameters", NULL, 0);
    write_value(ecPrivateKey, "publicKey", point + pointLength - 65, 65 * 8);
    unsigned char *inner;
    int innerLength = encode("ec private key", ecPrivateKey, "", &inner);
    printf("create pkcs8 %d\n", asn1_create_element(definitions, "WebCrypto.PrivateKeyInfo", &pkcs8));
    write_value(pkcs8, "version", "0", 0);
    write_value(pkcs8, "privateKeyAlgorithm.algorithm", ecPublicKeyIdentifier, 1);
    write_value(pkcs8, "privateKeyAlgorithm.parameters", parameters, parametersLength);
    write_value(pkcs8, "privateKey", inner, innerLength);
    write_value(pkcs8, "attributes", NULL, 0);
    unsigned char *pkcs8Der;
    encode("ec pkcs8", pkcs8, "", &pkcs8Der);
    free(pkcs8Der);
    /* With the attributes still there (no value): an error. */
    asn1_node incomplete = NULL;
    asn1_create_element(definitions, "WebCrypto.PrivateKeyInfo", &incomplete);
    write_value(incomplete, "version", "0", 0);
    encode("incomplete pkcs8", incomplete, "", &pkcs8Der);
    free(pkcs8Der);
    asn1_delete_structure(&incomplete);

    /* Ed25519 as CryptoKeyOKP exports it. */
    asn1_node curveKey = NULL, okp = NULL;
    asn1_create_element(definitions, "WebCrypto.CurvePrivateKey", &curveKey);
    write_value(curveKey, "", d, 32);
    unsigned char *curveDer;
    int curveLength = encode("curve private key", curveKey, "", &curveDer);
    asn1_create_element(definitions, "WebCrypto.PrivateKeyInfo", &okp);
    write_value(okp, "version", "0", 0);
    write_value(okp, "privateKeyAlgorithm.algorithm", "1.3.101.112", 1);
    write_value(okp, "privateKeyAlgorithm.parameters", NULL, 0);
    write_value(okp, "privateKey", curveDer, curveLength);
    write_value(okp, "attributes", NULL, 0);
    encode("ed25519 pkcs8", okp, "", &pkcs8Der);
    free(pkcs8Der);
    free(curveDer);

    /* RSA's public key and private key. */
    asn1_node rsaPublic = NULL, rsaPrivate = NULL;
    asn1_create_element(definitions, "WebCrypto.RSAPublicKey", &rsaPublic);
    unsigned char modulus[] = { 0x00, 0xC0, 0xFF, 0xEE, 0x01 };
    unsigned char exponent[] = { 0x01, 0x00, 0x01 };
    unsigned char padded[] = { 0x00, 0x00, 0x00, 0x7F };
    unsigned char negative[] = { 0xFF, 0xFF, 0x80 };
    write_value(rsaPublic, "modulus", modulus, sizeof modulus);
    write_value(rsaPublic, "publicExponent", exponent, sizeof exponent);
    unsigned char *rsaDer;
    encode("rsa public key", rsaPublic, "", &rsaDer);
    free(rsaDer);
    write_value(rsaPublic, "modulus", padded, sizeof padded);
    write_value(rsaPublic, "publicExponent", negative, sizeof negative);
    encode("rsa public key, integers made minimal", rsaPublic, "", &rsaDer);
    free(rsaDer);
    write_value(rsaPublic, "publicExponent", "-129", 0);
    encode("rsa public key, a decimal", rsaPublic, "", &rsaDer);
    free(rsaDer);
    write_value(rsaPublic, "publicExponent", "65537", 0);
    element(rsaPublic, "publicExponent");
    write_value(rsaPublic, "publicExponent", "x", 0);
    write_value(rsaPublic, "nothing", "1", 0);
    write_value(rsaPublic, "modulus", NULL, 0);
    asn1_create_element(definitions, "WebCrypto.RSAPrivateKey", &rsaPrivate);
    const char *names[] = { "version", "modulus", "publicExponent", "privateExponent", "prime1", "prime2", "exponent1", "exponent2", "coefficient" };
    for (int n = 0; n < 9; n++) {
        unsigned char value[2] = { 0x11 * (n + 1), (unsigned char)n };
        write_value(rsaPrivate, names[n], n == 0 ? (const void *)"0" : value, n == 0 ? 0 : 2);
    }
    encode("rsa private key with other primes unset", rsaPrivate, "", &rsaDer);
    free(rsaDer);
    write_value(rsaPrivate, "otherPrimeInfos", NULL, 0);
    encode("rsa private key", rsaPrivate, "", &rsaDer);
    free(rsaDer);
    /* A SEQUENCE OF: two NEW elements. */
    asn1_node others = NULL;
    asn1_create_element(definitions, "WebCrypto.OtherPrimeInfos", &others);
    write_value(others, "", "NEW", 1);
    write_value(others, "?1.prime", "3", 0);
    write_value(others, "?1.exponent", "5", 0);
    write_value(others, "?1.coefficient", "7", 0);
    write_value(others, "", "NEW", 1);
    write_value(others, "?LAST.prime", "11", 0);
    write_value(others, "?2.exponent", "13", 0);
    write_value(others, "?2.coefficient", "17", 0);
    write_value(others, "", "OLD", 1);
    int othersLength = encode("other prime infos", others, "", &rsaDer);
    element(others, "?2.exponent");
    asn1_node back = decode("other prime infos back", "WebCrypto.OtherPrimeInfos", rsaDer, othersLength);
    element(back, "?1.prime");
    element(back, "?2.coefficient");
    element(back, "?3.prime");
    asn1_delete_structure(&back);
    free(rsaDer);

    /* Attributes: a SET OF, in DER's order. */
    asn1_node attributes = NULL;
    asn1_create_element(definitions, "WebCrypto.Attributes", &attributes);
    write_value(attributes, "", "NEW", 1);
    write_value(attributes, "?1.type", "1.2.3.4", 1);
    write_value(attributes, "?1.values", "NEW", 1);
    unsigned char octets[] = { 0x04, 0x01, 0x42 };
    write_value(attributes, "?1.values.?1", octets, sizeof octets);
    write_value(attributes, "", "NEW", 1);
    write_value(attributes, "?2.type", "1.2.3", 1);
    write_value(attributes, "?2.values", "NEW", 1);
    unsigned char integer[] = { 0x02, 0x01, 0x05 };
    write_value(attributes, "?2.values.?1", integer, sizeof integer);
    encode("attributes", attributes, "", &rsaDer);
    free(rsaDer);

    asn1_delete_structure(&attributes);
    asn1_delete_structure(&others);
    asn1_delete_structure(&rsaPublic);
    asn1_delete_structure(&rsaPrivate);
    asn1_delete_structure(&curveKey);
    asn1_delete_structure(&okp);
    asn1_delete_structure(&original);
    asn1_delete_structure(&ecPrivateKey);
    asn1_delete_structure(&pkcs8);
    asn1_delete_structure(&spki);
    asn1_delete_structure(&ecParameters);
    free(parameters);
    free(inner);
    free(keyDer);
    free(point);
}

int main(void)
{
    char error[ASN1_MAX_ERROR_DESCRIPTION_SIZE] = "";
    int ret = asn1_array2tree(definitions_array, &definitions, error);
    printf("array2tree %d \"%s\"\n", ret, error);
    asn1_node again = definitions;
    printf("again %d\n", asn1_array2tree(definitions_array, &again, error));
    static const asn1_static_node broken[] = {
        { "Broken", 536872976, NULL },
        { NULL, 1073741836, NULL },
        { "Thing", 1610612741, NULL },
        { "part", 2, "Missing" },
        { NULL, 0, NULL }
    };
    asn1_node brokenTree = NULL;
    ret = asn1_array2tree(broken, &brokenTree, error);
    printf("broken %d \"%s\" %s\n", ret, error, brokenTree ? "set" : "NULL");
    static const asn1_static_node missing[] = {
        { "Missing", 536872976, NULL },
        { NULL, 1073741836, NULL },
        { "Thing", 536870917, NULL },
        { "part", 2, "Nowhere" },
        { NULL, 0, NULL }
    };
    ret = asn1_array2tree(missing, &brokenTree, error);
    printf("missing %d \"%s\" %s\n", ret, error, brokenTree ? "set" : "NULL");
    decoding();
    encoding();
    asn1_delete_structure(&definitions);
    printf("definitions %s\n", definitions ? "set" : "NULL");
    return 0;
}
