/* iconv and gettext (iconv.h, libintl.h), printed so that the program
   linked against mlxlibc and against glibc can be compared line by line
   (tools/check_iconv.sh). Each conversion prints iconv's result, errno,
   the input consumed and the output made, then what the reset call
   writes. */
#include <errno.h>
#include <iconv.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static void convert(const char *to, const char *from, const char *in, size_t length, size_t room)
{
    iconv_t cd = iconv_open(to, from);
    if (cd == (iconv_t)-1) {
        printf("%s <- %s: no conversion (errno %d)\n", to, from, errno);
        return;
    }
    char out[256];
    char *input = (char *)in, *output = out;
    size_t inputLeft = length, outputLeft = room;
    errno = 0;
    size_t result = iconv(cd, &input, &inputLeft, &output, &outputLeft);
    int error = result == (size_t)-1 ? errno : 0;
    printf("%s <- %s: %ld errno %d took %zu made ", to, from, (long)result, error, length - inputLeft);
    for (char *p = out; p < output; p++)
        printf("%02x", (unsigned char)*p);
    char *reset = output;
    size_t resetLeft = outputLeft;
    result = iconv(cd, NULL, NULL, &reset, &resetLeft);
    printf(" reset %ld", (long)result);
    for (char *p = output; p < reset; p++)
        printf("%02x", (unsigned char)*p);
    printf("\n");
    if (iconv_close(cd) != 0)
        printf("iconv_close fails\n");
}

/* The same text in pieces through one descriptor: what is left over at
   the end of a piece goes in front of the next. */
static void pieces(const char *to, const char *from, const char *in, size_t length, size_t piece)
{
    iconv_t cd = iconv_open(to, from);
    char out[256], pending[16];
    char *output = out;
    size_t outputLeft = sizeof out, pendingLength = 0, at = 0;
    while (at < length || pendingLength > 0) {
        char buffer[32];
        memcpy(buffer, pending, pendingLength);
        size_t take = length - at < piece ? length - at : piece;
        memcpy(buffer + pendingLength, in + at, take);
        at += take;
        char *input = buffer;
        size_t inputLeft = pendingLength + take;
        size_t result = iconv(cd, &input, &inputLeft, &output, &outputLeft);
        if (result == (size_t)-1 && errno != EINVAL) {
            printf("pieces %s <- %s: errno %d\n", to, from, errno);
            break;
        }
        memcpy(pending, input, inputLeft);
        pendingLength = inputLeft;
        if (take == 0)
            break;
    }
    printf("pieces %s <- %s by %zu: left %zu made ", to, from, piece, pendingLength);
    for (char *p = out; p < output; p++)
        printf("%02x", (unsigned char)*p);
    printf("\n");
    iconv_close(cd);
}

/* Every byte of a single-byte charset as UTF-8 and back. */
static void charset(const char *name, const char *skip)
{
    iconv_t decode = iconv_open("UTF-8", name);
    iconv_t encode = iconv_open(name, "UTF-8");
    if (decode == (iconv_t)-1 || encode == (iconv_t)-1) {
        printf("%s: no conversion\n", name);
        return;
    }
    printf("%s:", name);
    for (int byte = 128; byte < 256; byte++) {
        if (skip && strchr(skip, byte))
            continue;
        char in[1] = { (char)byte }, out[8], back[8];
        char *input = in, *output = out;
        size_t inputLeft = 1, outputLeft = sizeof out;
        if (iconv(decode, &input, &inputLeft, &output, &outputLeft) == (size_t)-1) {
            printf(" %02x-", byte);
            continue;
        }
        size_t made = output - out;
        printf(" %02x=", byte);
        for (size_t i = 0; i < made; i++)
            printf("%02x", (unsigned char)out[i]);
        input = out;
        inputLeft = made;
        output = back;
        outputLeft = sizeof back;
        if (iconv(encode, &input, &inputLeft, &output, &outputLeft) == (size_t)-1 || output - back != 1 || (unsigned char)back[0] != byte)
            printf("!");
    }
    printf("\n");
    iconv_close(decode);
    iconv_close(encode);
}

int main(void)
{
    setlocale(LC_ALL, "C");

    /* The Unicode forms, and their byte order marks. */
    convert("UTF-16", "UTF-8", "A\xc3\xa9", 3, 64);
    convert("UTF-32", "UTF-8", "A", 1, 64);
    convert("UCS-2", "UTF-8", "A", 1, 64);
    convert("UCS-4", "UTF-8", "A", 1, 64);
    convert("WCHAR_T", "UTF-8", "A", 1, 64);
    convert("UTF-16BE", "UTF-8", "A\xc3\xa9\xe2\x82\xac", 6, 64);
    convert("UTF-32BE", "UTF-8", "A\xf0\x9f\x98\x80", 5, 64);
    convert("UTF-32LE", "UTF-8", "A\xf0\x9f\x98\x80", 5, 64);
    convert("UTF-8", "UTF-16", "\xff\xfe" "A\0", 4, 64);
    convert("UTF-8", "UTF-16", "\xfe\xff\0A", 4, 64);
    convert("UTF-8", "UTF-16", "\0A", 2, 64);
    convert("UTF-8", "UTF-16", "A\0", 2, 64);
    convert("UTF-8", "UTF-16", "\xff\xfe", 2, 64);
    convert("UTF-8", "UTF-32", "A\0\0\0", 4, 64);
    convert("UTF-8", "UTF-32", "\0\0\xfe\xff\0\0\0A", 8, 64);
    convert("UTF-8", "UCS-2", "A\0", 2, 64);
    convert("UTF-8", "UCS-2BE", "\0A\x20\xac", 4, 64);
    convert("UTF-8", "UTF-16LE", "\xff\xfe" "A\0", 4, 64);
    convert("UTF-8", "UTF-16LE", "=\xd8\0\xde", 4, 64);
    convert("UTF-8", "UTF-16BE", "\xd8=\xde\0", 4, 64);
    convert("UTF-16LE", "UTF-8", "\xf0\x9f\x98\x80", 4, 64);
    convert("UTF-8", "UTF-8", "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", 10, 64);
    convert("UTF-8", "WCHAR_T", "A\0\0\0\xac\x20\0\0", 8, 64);

    /* Invalid and incomplete input, a full output. */
    convert("UTF-8", "UTF-8", "A\xff" "B", 3, 64);
    convert("UTF-8", "UTF-8", "A\xc3", 2, 64);
    convert("UTF-8", "UTF-8", "A\xe2\x82", 3, 64);
    convert("UTF-8", "UTF-8", "\xed\xa0\x80", 3, 64);
    convert("UTF-8", "UTF-8", "\xc0\x80", 2, 64);
    convert("UTF-8", "UTF-8", "\xe0\x80\x80", 3, 64);
    convert("UTF-8", "UTF-8", "\xe2\x28\xa1", 3, 64);
    convert("UTF-8", "UTF-16LE", "\x00\xd8" "A\0", 4, 64);
    convert("UTF-8", "UTF-16LE", "\x00\xd8", 2, 64);
    convert("UTF-8", "UTF-16LE", "\x00\xdc" "A\0", 4, 64);
    convert("UTF-8", "UTF-16LE", "A\0B", 3, 64);
    convert("UTF-8", "UTF-32LE", "\x00\xd8\0\0", 4, 64);
    convert("UTF-8", "UTF-32LE", "\0\0\x11\0", 4, 64);
    convert("UTF-8", "UTF-32LE", "A\0\0", 3, 64);
    convert("UTF-8", "UCS-2LE", "\x00\xd8", 2, 64);
    convert("UCS-2", "UTF-8", "\xf0\x9f\x98\x80", 4, 64);
    convert("UTF-16LE", "UTF-8", "ABCD", 4, 5);
    convert("UTF-8", "UTF-16LE", "A\0\xac\x20", 4, 3);
    convert("UTF-16", "UTF-8", "A", 1, 3);

    /* ISO-8859-1 and ASCII, what they cannot encode, TRANSLIT and IGNORE. */
    convert("ISO-8859-1", "UTF-8", "A\xc3\xa9" "B", 4, 64);
    convert("ISO-8859-1", "UTF-8", "A\xe2\x82\xac" "B", 5, 64);
    convert("ISO-8859-1//TRANSLIT", "UTF-8", "A\xe2\x82\xac" "B", 5, 64);
    convert("ISO-8859-1//IGNORE", "UTF-8", "A\xe2\x82\xac" "B", 5, 64);
    convert("ASCII", "UTF-8", "A\xc3\xa9" "B", 4, 64);
    convert("ASCII//TRANSLIT", "UTF-8", "A\xc3\xa9" "B", 4, 64);
    convert("ASCII//TRANSLIT", "UTF-8", "\xe2\x80\x9c" "x\xe2\x80\x9d" "\xc3\x9f", 9, 64);
    convert("ASCII//TRANSLIT", "UTF-8", "\xe2\x80\x98" "a\xe2\x80\x99 \xe2\x80\x93 \xe2\x80\xa6 \xc2\xa0 \xc3\x86\xc5\x93", 23, 64);
    convert("ASCII//TRANSLIT//IGNORE", "UTF-8", "a\xffz", 3, 64);
    convert("UTF-8//IGNORE", "UTF-8", "A\xff" "B", 3, 64);
    convert("UTF-8", "ASCII", "A\xe9" "B", 3, 64);
    convert("UTF-8", "ISO-8859-1", "A\xe9\x80" "B", 4, 64);
    convert("UTF-16LE", "ISO-8859-1", "\xff", 1, 64);

    /* Names. */
    convert("utf8", "Latin1", "\xe9", 1, 64);
    convert("UTF-8", "ISO_8859-1:1987", "\xe9", 1, 64);
    convert("UTF-8", "ISO8859-1", "\xe9", 1, 64);
    convert("UTF-8", "L1", "\xe9", 1, 64);
    convert("UTF-8", "US-ASCII", "A", 1, 64);
    convert("UTF-8", "ANSI_X3.4-1968", "A", 1, 64);
    convert("UTF-8", "UNICODELITTLE", "A\0", 2, 64);
    convert("UTF-8", "UNICODEBIG", "\0A", 2, 64);
    convert("UTF-8", "UTF-16//", "A\0", 2, 64);
    convert("UTF-8", "", "A", 1, 64);
    convert("UTF-8", "no-such-charset", "x", 1, 64);
    convert("no-such-charset", "UTF-8", "x", 1, 64);

    /* Pieces, a character cut between them. */
    pieces("UTF-16LE", "UTF-8", "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z", 11, 1);
    pieces("UTF-16LE", "UTF-8", "a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80z", 11, 3);
    pieces("UTF-8", "UTF-16", "\xff\xfe" "a\0=\xd8\0\xdez\0", 10, 1);
    pieces("UTF-8", "UTF-32BE", "\0\0\0a\0\x01\xf6\0", 8, 3);

    /* The single-byte charsets: every byte, there and back. The bytes
       left out are where the Encoding Standard's tables (mlxlibc's) are
       not glibc's: the bytes a Windows code page leaves undefined, which
       the Encoding Standard reads as the C1 controls of their values;
       KOI8-U's Belarusian letters (KOI8-RU's, which the Encoding Standard
       has in KOI8-U); and in MACINTOSH the increment sign and the Apple
       logo (glibc has the older Delta and a private use character).
       glibc's CP1255 and CP1258 compose a letter with the marks after it,
       so a byte is not a character there: they are not compared. */
    charset("ISO-8859-2", NULL);
    charset("ISO-8859-3", NULL);
    charset("ISO-8859-4", NULL);
    charset("ISO-8859-5", NULL);
    charset("ISO-8859-6", NULL);
    charset("ISO-8859-7", NULL);
    charset("ISO-8859-8", NULL);
    charset("ISO-8859-9", NULL);
    charset("ISO-8859-10", NULL);
    charset("ISO-8859-13", NULL);
    charset("ISO-8859-14", NULL);
    charset("ISO-8859-15", NULL);
    charset("ISO-8859-16", NULL);
    charset("KOI8-R", NULL);
    charset("KOI8-U", "\xae\xbe");
    charset("CP866", NULL);
    charset("CP1250", "\x81\x83\x88\x90\x98");
    charset("CP1251", "\x98");
    charset("CP1252", "\x81\x8d\x8f\x90\x9d");
    charset("CP1253", "\x81\x88\x8a\x8c\x8d\x8e\x8f\x90\x98\x9a\x9c\x9d\x9e\x9f");
    charset("CP1254", "\x81\x8d\x8e\x8f\x90\x9d\x9e");
    charset("CP1257", "\x81\x83\x88\x8a\x8c\x90\x98\x9a\x9c\x9f");
    charset("MACINTOSH", "\xc6\xf0");
    charset("windows-1250", "\x81\x83\x88\x90\x98");
    charset("latin2", NULL);

    /* gettext: no catalogs, the messages as written. */
    const char *message = "Hello";
    printf("gettext %s %d\n", gettext(message), gettext(message) == message);
    printf("dgettext %s\n", dgettext("WebKit", "Open"));
    printf("dcgettext %s\n", dcgettext("WebKit", "Close", LC_MESSAGES));
    printf("ngettext %s %s %s\n", ngettext("file", "files", 0), ngettext("file", "files", 1), ngettext("file", "files", 2));
    printf("dngettext %s\n", dngettext("WebKit", "tab", "tabs", 3));
    printf("dcngettext %s\n", dcngettext(NULL, "page", "pages", 1, LC_MESSAGES));
    printf("textdomain %s\n", textdomain(NULL));
    printf("textdomain %s\n", textdomain("WebKit"));
    printf("textdomain %s\n", textdomain(NULL));
    printf("textdomain %s\n", textdomain(""));
    printf("bindtextdomain %s\n", bindtextdomain("WebKit", NULL));
    printf("bindtextdomain %s\n", bindtextdomain("WebKit", "/opt/webkit/locale"));
    printf("bindtextdomain %s\n", bindtextdomain("WebKit", NULL));
    const char *codeset = bind_textdomain_codeset("WebKit", NULL);
    printf("bind_textdomain_codeset %s\n", codeset ? codeset : "(none)");
    printf("bind_textdomain_codeset %s\n", bind_textdomain_codeset("WebKit", "UTF-8"));
    printf("bind_textdomain_codeset %s\n", bind_textdomain_codeset("WebKit", NULL));
    return 0;
}
