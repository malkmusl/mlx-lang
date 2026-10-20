/* ucsdet: what charsets the detector finds for texts of known ones:
   the first match and its confidence, and what getUChars gives. */
#include <stdio.h>
#include <string.h>
#include <unicode/ucsdet.h>
#include <unicode/ustring.h>

static void detect(const char *what, const char *bytes, int length)
{
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector *detector = ucsdet_open(&status);
    ucsdet_setText(detector, bytes, length, &status);
    int32_t found = 0;
    const UCharsetMatch **matches = ucsdet_detectAll(detector, &found, &status);
    printf("%s:", what);
    for (int i = 0; i < found && i < 3; i++)
        printf(" %s %d%s%s", ucsdet_getName(matches[i], &status), ucsdet_getConfidence(matches[i], &status), *ucsdet_getLanguage(matches[i], &status) ? "/" : "", ucsdet_getLanguage(matches[i], &status));
    const UCharsetMatch *best = ucsdet_detect(detector, &status);
    UChar text[256];
    UErrorCode convert = U_ZERO_ERROR;
    int n = best ? ucsdet_getUChars(best, text, 256, &convert) : 0;
    printf(" (%d units%s%s)\n", n, U_FAILURE(convert) ? " " : "", U_FAILURE(convert) ? u_errorName(convert) : "");
    if (U_FAILURE(status))
        printf("  %s\n", u_errorName(status));
    ucsdet_close(detector);
}

int main(void)
{
    { static const char text[] = "\x47" "\x72" "\xc3" "\xbc" "\xc3" "\x9f" "\x65" "\x20" "\x61" "\x75" "\x73" "\x20" "\x4b" "\xc3" "\xb6" "\x6c" "\x6e" "\x20" "\xe2" "\x80" "\x94" "\x20" "\x64" "\x61" "\x73" "\x20" "\x69" "\x73" "\x74" "\x20" "\x65" "\x69" "\x6e" "\x20" "\x54" "\x65" "\x78" "\x74" "\x20" "\x6d" "\x69" "\x74" "\x20" "\x55" "\x6d" "\x6c" "\x61" "\x75" "\x74" "\x65" "\x6e" "\x3a" "\x20" "\xc3" "\xa4" "\xc3" "\xb6" "\xc3" "\xbc" "\xc3" "\x9f" "\x2e"; detect("utf-8", text, (int)sizeof text - 1); }
    { static const char text[] = "\xff" "\xfe" "\x48" "\x00" "\x65" "\x00" "\x6c" "\x00" "\x6c" "\x00" "\x6f" "\x00" "\x2c" "\x00" "\x20" "\x00" "\x77" "\x00" "\x6f" "\x00" "\x72" "\x00" "\x6c" "\x00" "\x64" "\x00" "\x2e" "\x00" "\x20" "\x00" "\x54" "\x00" "\x68" "\x00" "\x69" "\x00" "\x73" "\x00" "\x20" "\x00" "\x69" "\x00" "\x73" "\x00" "\x20" "\x00" "\x55" "\x00" "\x54" "\x00" "\x46" "\x00" "\x2d" "\x00" "\x31" "\x00" "\x36" "\x00" "\x2e" "\x00"; detect("utf-16le with BOM", text, (int)sizeof text - 1); }
    { static const char text[] = "\x00" "\x50" "\x00" "\x6c" "\x00" "\x61" "\x00" "\x69" "\x00" "\x6e" "\x00" "\x20" "\x00" "\x41" "\x00" "\x53" "\x00" "\x43" "\x00" "\x49" "\x00" "\x49" "\x00" "\x20" "\x00" "\x74" "\x00" "\x65" "\x00" "\x78" "\x00" "\x74" "\x00" "\x20" "\x00" "\x69" "\x00" "\x6e" "\x00" "\x20" "\x00" "\x55" "\x00" "\x54" "\x00" "\x46" "\x00" "\x2d" "\x00" "\x31" "\x00" "\x36" "\x00" "\x42" "\x00" "\x45"; detect("utf-16be without BOM", text, (int)sizeof text - 1); }
    { static const char text[] = "\xff" "\xfe" "\x00" "\x00" "\x48" "\x00" "\x00" "\x00" "\x69" "\x00" "\x00" "\x00" "\x20" "\x00" "\x00" "\x00" "\x74" "\x00" "\x00" "\x00" "\x68" "\x00" "\x00" "\x00" "\x65" "\x00" "\x00" "\x00" "\x72" "\x00" "\x00" "\x00" "\x65" "\x00" "\x00" "\x00"; detect("utf-32le with BOM", text, (int)sizeof text - 1); }
    { static const char text[] = "\x82" "\xb1" "\x82" "\xf1" "\x82" "\xc9" "\x82" "\xbf" "\x82" "\xcd" "\x81" "\x41" "\x90" "\xa2" "\x8a" "\x45" "\x81" "\x42" "\x82" "\xb1" "\x82" "\xea" "\x82" "\xcd" "\x93" "\xfa" "\x96" "\x7b" "\x8c" "\xea" "\x82" "\xcc" "\x83" "\x65" "\x83" "\x4c" "\x83" "\x58" "\x83" "\x67" "\x82" "\xc5" "\x82" "\xb7" "\x81" "\x42" "\x82" "\xd0" "\x82" "\xe7" "\x82" "\xaa" "\x82" "\xc8" "\x82" "\xc6" "\x83" "\x4a" "\x83" "\x5e" "\x83" "\x4a" "\x83" "\x69" "\x82" "\xc6" "\x8a" "\xbf" "\x8e" "\x9a" "\x82" "\xaa" "\x82" "\xa0" "\x82" "\xe8" "\x82" "\xdc" "\x82" "\xb7" "\x81" "\x42"; detect("shift_jis", text, (int)sizeof text - 1); }
    { static const char text[] = "\xa4" "\xb3" "\xa4" "\xf3" "\xa4" "\xcb" "\xa4" "\xc1" "\xa4" "\xcf" "\xa1" "\xa2" "\xc0" "\xa4" "\xb3" "\xa6" "\xa1" "\xa3" "\xa4" "\xb3" "\xa4" "\xec" "\xa4" "\xcf" "\xc6" "\xfc" "\xcb" "\xdc" "\xb8" "\xec" "\xa4" "\xce" "\xa5" "\xc6" "\xa5" "\xad" "\xa5" "\xb9" "\xa5" "\xc8" "\xa4" "\xc7" "\xa4" "\xb9" "\xa1" "\xa3" "\xa4" "\xd2" "\xa4" "\xe9" "\xa4" "\xac" "\xa4" "\xca" "\xa4" "\xc8" "\xa5" "\xab" "\xa5" "\xbf" "\xa5" "\xab" "\xa5" "\xca" "\xa4" "\xc8" "\xb4" "\xc1" "\xbb" "\xfa" "\xa4" "\xac" "\xa4" "\xa2" "\xa4" "\xea" "\xa4" "\xde" "\xa4" "\xb9" "\xa1" "\xa3"; detect("euc-jp", text, (int)sizeof text - 1); }
    { static const char text[] = "\xbe" "\xc8" "\xb3" "\xe7" "\xc7" "\xcf" "\xbc" "\xbc" "\xbf" "\xe4" "\x2e" "\x20" "\xc0" "\xcc" "\xb0" "\xcd" "\xc0" "\xba" "\x20" "\xc7" "\xd1" "\xb1" "\xb9" "\xbe" "\xee" "\x20" "\xc5" "\xd8" "\xbd" "\xba" "\xc6" "\xae" "\xc0" "\xd4" "\xb4" "\xcf" "\xb4" "\xd9" "\x2e" "\x20" "\xc7" "\xd1" "\xb1" "\xdb" "\xb7" "\xce" "\x20" "\xbe" "\xb4" "\x20" "\xb9" "\xae" "\xc0" "\xe5" "\xc0" "\xcc" "\x20" "\xbf" "\xa9" "\xb7" "\xaf" "\x20" "\xb0" "\xb3" "\x20" "\xc0" "\xd6" "\xbd" "\xc0" "\xb4" "\xcf" "\xb4" "\xd9" "\x2e"; detect("euc-kr", text, (int)sizeof text - 1); }
    { static const char text[] = "\x1b" "\x24" "\x42" "\x24" "\x33" "\x24" "\x73" "\x24" "\x4b" "\x24" "\x41" "\x24" "\x4f" "\x21" "\x22" "\x40" "\x24" "\x33" "\x26" "\x21" "\x23" "\x24" "\x33" "\x24" "\x6c" "\x24" "\x4f" "\x46" "\x7c" "\x4b" "\x5c" "\x38" "\x6c" "\x24" "\x4e" "\x25" "\x46" "\x25" "\x2d" "\x25" "\x39" "\x25" "\x48" "\x24" "\x47" "\x24" "\x39" "\x21" "\x23" "\x1b" "\x28" "\x42"; detect("iso-2022-jp", text, (int)sizeof text - 1); }
    { static const char text[] = "\xc7" "\x61" "\x20" "\x76" "\x61" "\x20" "\x74" "\x72" "\xe8" "\x73" "\x20" "\x62" "\x69" "\x65" "\x6e" "\x2c" "\x20" "\x6d" "\x65" "\x72" "\x63" "\x69" "\x2e" "\x20" "\x4f" "\xf9" "\x20" "\x65" "\x73" "\x74" "\x20" "\x6c" "\x61" "\x20" "\x62" "\x69" "\x62" "\x6c" "\x69" "\x6f" "\x74" "\x68" "\xe8" "\x71" "\x75" "\x65" "\x3f" "\x20" "\xc0" "\x20" "\x62" "\x69" "\x65" "\x6e" "\x74" "\xf4" "\x74" "\x21"; detect("iso-8859-1", text, (int)sizeof text - 1); }
    { static const char text[] = "\x48" "\x65" "\x20" "\x73" "\x61" "\x69" "\x64" "\x20" "\x93" "\x68" "\x65" "\x6c" "\x6c" "\x6f" "\x94" "\x20" "\x96" "\x20" "\x61" "\x6e" "\x64" "\x20" "\x6c" "\x65" "\x66" "\x74" "\x85" "\x20" "\x66" "\x6f" "\x72" "\x20" "\x80" "\x35" "\x2e"; detect("windows-1252", text, (int)sizeof text - 1); }
    { static const char text[] = "\x4a" "\x75" "\x73" "\x74" "\x20" "\x70" "\x6c" "\x61" "\x69" "\x6e" "\x20" "\x41" "\x53" "\x43" "\x49" "\x49" "\x20" "\x74" "\x65" "\x78" "\x74" "\x2c" "\x20" "\x6e" "\x6f" "\x74" "\x68" "\x69" "\x6e" "\x67" "\x20" "\x65" "\x6c" "\x73" "\x65" "\x20" "\x68" "\x65" "\x72" "\x65" "\x20" "\x61" "\x74" "\x20" "\x61" "\x6c" "\x6c" "\x2e"; detect("ascii", text, (int)sizeof text - 1); }
    UErrorCode status = U_ZERO_ERROR;
    UCharsetDetector *detector = ucsdet_open(&status);
    UEnumeration *all = ucsdet_getAllDetectableCharsets(detector, &status);
    printf("detectable: %d\n", uenum_count(all, &status));
    uenum_close(all);
    ucsdet_close(detector);
    return 0;
}
