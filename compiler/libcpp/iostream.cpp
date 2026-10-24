// std::ostream and std::istream (include/c++/iostream) over C's stdio:
// cout writes to stdout, cerr and clog to stderr, cin reads stdin.
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace std {

ostream cout = { {}, 1, 6, false, 0, 0, 0, 0 };
ostream cerr = { {}, 2, 6, false, 0, 0, 0, 0 };
ostream clog = { {}, 2, 6, false, 0, 0, 0, 0 };
istream cin = { {}, 0, false, false };

static FILE *fileOf(int which) {
    if (which == 1) return stdout;
    if (which == 2) return stderr;
    return stdin;
}

ostream &ostream::write(const char *bytes, size_t count) {
    if (sink) {
        sink->append(bytes, count);
        return *this;
    }
    if (fwrite(bytes, 1, count, fileOf(which)) != count) failed = true;
    return *this;
}

ostream &ostream::put(char c) {
    if (sink) {
        sink->push_back(c);
        return *this;
    }
    if (fputc(c, fileOf(which)) < 0) failed = true;
    return *this;
}

ostream &ostream::flush() {
    if (!sink) fflush(fileOf(which));
    return *this;
}

ostream &ostream::padded(const char *text, size_t count, size_t prefix) {
    long width = fieldWidth;
    fieldWidth = 0;
    if (width <= 0 || (size_t)width <= count) return write(text, count);
    size_t pad = (size_t)width - count;
    char c = fill();
    unsigned adjust = format & ios_base::adjustfield;
    if (adjust == ios_base::left) {
        write(text, count);
        for (size_t i = 0; i < pad; i++) put(c);
        return *this;
    }
    if (adjust == ios_base::internal) {
        write(text, prefix);
        for (size_t i = 0; i < pad; i++) put(c);
        return write(text + prefix, count - prefix);
    }
    for (size_t i = 0; i < pad; i++) put(c);
    return write(text, count);
}

ostream &ostream::operator<<(const char *text) { return padded(text, strlen(text), 0); }

ostream &ostream::operator<<(char c) { return padded(&c, 1, 0); }

ostream &ostream::operator<<(bool value) {
    if (format & ios_base::boolalpha) return value ? padded("true", 4, 0) : padded("false", 5, 0);
    return padded(value ? "1" : "0", 1, 0);
}

// An integer as the flags say: its base (with 0x or 0 for showbase), the
// letters' case, a + for showpos.
static size_t formatInteger(char *out, unsigned long magnitude, bool negative, unsigned format, size_t *prefix) {
    unsigned base = format & ios_base::basefield;
    unsigned radix = base == ios_base::hex ? 16 : base == ios_base::oct ? 8 : 10;
    const char *digitsOf = (format & ios_base::uppercase) ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[80];
    size_t n = 0;
    do {
        reversed[n++] = digitsOf[magnitude % radix];
        magnitude /= radix;
    } while (magnitude != 0);
    size_t at = 0;
    if (negative) out[at++] = '-';
    else if (radix == 10 && (format & ios_base::showpos)) out[at++] = '+';
    if ((format & ios_base::showbase) && radix == 16 && !(n == 1 && reversed[0] == '0')) {
        out[at++] = '0';
        out[at++] = (format & ios_base::uppercase) ? 'X' : 'x';
    }
    if ((format & ios_base::showbase) && radix == 8 && reversed[n - 1] != '0') out[at++] = '0';
    *prefix = at;
    while (n > 0) out[at++] = reversed[--n];
    return at;
}

ostream &ostream::operator<<(long value) {
    char buffer[96];
    size_t prefix = 0;
    unsigned base = format & ios_base::basefield;
    bool negative = value < 0 && base != ios_base::hex && base != ios_base::oct;
    unsigned long magnitude = negative ? 0UL - (unsigned long)value : (unsigned long)value;
    size_t n = formatInteger(buffer, magnitude, negative, format, &prefix);
    return padded(buffer, n, prefix);
}

ostream &ostream::operator<<(unsigned long value) {
    char buffer[96];
    size_t prefix = 0;
    size_t n = formatInteger(buffer, value, false, format, &prefix);
    return padded(buffer, n, prefix);
}

ostream &ostream::operator<<(double value) {
    char spec[16];
    size_t at = 0;
    spec[at++] = '%';
    if (format & ios_base::showpos) spec[at++] = '+';
    if (format & ios_base::showpoint) spec[at++] = '#';
    spec[at++] = '.';
    spec[at++] = '*';
    unsigned floating = format & ios_base::floatfield;
    char conversion = floating == ios_base::fixed ? 'f' : floating == ios_base::scientific ? 'e' : floating == ios_base::floatfield ? 'a' : 'g';
    if (format & ios_base::uppercase) conversion = (char)toupper(conversion);
    spec[at++] = conversion;
    spec[at] = 0;
    char buffer[400];
    int n = snprintf(buffer, sizeof buffer, spec, digits, value);
    size_t prefix = (buffer[0] == '-' || buffer[0] == '+') ? 1 : 0;
    return padded(buffer, (size_t)n, prefix);
}

ostream &ostream::operator<<(const void *pointer) {
    char buffer[32];
    int n = snprintf(buffer, sizeof buffer, "%p", pointer);
    return padded(buffer, (size_t)n, 0);
}

ostream &operator<<(ostream &out, const string &text) { return out.padded(text.data(), text.size(), 0); }

ostream &endl(ostream &out) {
    out.put('\n');
    return out.flush();
}

ostream &flush(ostream &out) { return out.flush(); }

ostream &ends(ostream &out) { return out.put(0); }

// ---------------------------------------------------------------------------

int istream::get() {
    int c = fgetc(fileOf(which));
    if (c < 0) {
        ended = true;
        failed = true;
    }
    return c;
}

istream &istream::get(char &c) {
    int read = get();
    if (read >= 0) c = (char)read;
    return *this;
}

int istream::peek() {
    int c = fgetc(fileOf(which));
    if (c < 0) {
        ended = true;
        return c;
    }
    ungetc(c, fileOf(which));
    return c;
}

istream &istream::unget() {
    fseek(fileOf(which), -1, SEEK_CUR);
    return *this;
}

istream &istream::ignore(size_t count, int delimiter) {
    while (count > 0) {
        int c = fgetc(fileOf(which));
        if (c < 0) {
            ended = true;
            break;
        }
        count--;
        if (c == delimiter) break;
    }
    return *this;
}

istream &istream::getline(char *buffer, size_t size, char delimiter) {
    size_t n = 0;
    while (1) {
        int c = fgetc(fileOf(which));
        if (c < 0) {
            ended = true;
            if (n == 0) failed = true;
            break;
        }
        if (c == delimiter) break;
        if (n + 1 >= size) {
            failed = true;
            ungetc(c, fileOf(which));
            break;
        }
        buffer[n++] = (char)c;
    }
    buffer[n] = 0;
    return *this;
}

istream &istream::read(char *buffer, size_t count) {
    if (fread(buffer, 1, count, fileOf(which)) != count) {
        ended = true;
        failed = true;
    }
    return *this;
}

// Whitespace before a value is passed; the first character of the value
// comes back (-1 at the end).
static int skipSpace(istream &in, int which) {
    while (1) {
        int c = fgetc(fileOf(which));
        if (c < 0) return -1;
        if (!isspace(c)) return c;
    }
}

// A word (to the next whitespace) into buffer, at most size - 1 chars.
static size_t readWord(istream &in, int which, char *buffer, size_t size) {
    int c = skipSpace(in, which);
    size_t n = 0;
    while (c >= 0 && !isspace(c)) {
        if (n + 1 < size) buffer[n++] = (char)c;
        c = fgetc(fileOf(which));
    }
    if (c >= 0) ungetc(c, fileOf(which));
    buffer[n] = 0;
    return n;
}

istream &istream::operator>>(char &c) {
    int read = skipSpace(*this, which);
    if (read < 0) {
        ended = true;
        failed = true;
    } else {
        c = (char)read;
    }
    return *this;
}

istream &istream::operator>>(char *word) {
    if (readWord(*this, which, word, 4096) == 0) {
        failed = true;
        ended = true;
    }
    return *this;
}

istream &istream::operator>>(long &value) {
    char buffer[64];
    if (readWord(*this, which, buffer, sizeof buffer) == 0) {
        failed = true;
        ended = true;
        return *this;
    }
    char *stop;
    long read = strtol(buffer, &stop, 10);
    if (stop == buffer) failed = true; else value = read;
    return *this;
}

istream &istream::operator>>(unsigned long &value) {
    char buffer[64];
    if (readWord(*this, which, buffer, sizeof buffer) == 0) {
        failed = true;
        ended = true;
        return *this;
    }
    char *stop;
    unsigned long read = strtoul(buffer, &stop, 10);
    if (stop == buffer) failed = true; else value = read;
    return *this;
}

istream &istream::operator>>(double &value) {
    char buffer[400];
    if (readWord(*this, which, buffer, sizeof buffer) == 0) {
        failed = true;
        ended = true;
        return *this;
    }
    char *stop;
    double read = strtod(buffer, &stop);
    if (stop == buffer) failed = true; else value = read;
    return *this;
}

istream &istream::operator>>(int &value) {
    long read = 0;
    *this >> read;
    if (!failed) value = (int)read;
    return *this;
}

istream &istream::operator>>(short &value) {
    long read = 0;
    *this >> read;
    if (!failed) value = (short)read;
    return *this;
}

istream &istream::operator>>(unsigned &value) {
    unsigned long read = 0;
    *this >> read;
    if (!failed) value = (unsigned)read;
    return *this;
}

istream &istream::operator>>(unsigned short &value) {
    unsigned long read = 0;
    *this >> read;
    if (!failed) value = (unsigned short)read;
    return *this;
}

istream &istream::operator>>(bool &value) {
    long read = 0;
    *this >> read;
    if (!failed) value = read != 0;
    return *this;
}

istream &istream::operator>>(float &value) {
    double read = 0;
    *this >> read;
    if (!failed) value = (float)read;
    return *this;
}

istream &operator>>(istream &in, string &word) {
    char buffer[4096];
    size_t n = readWord(in, in.which, buffer, sizeof buffer);
    if (n == 0) {
        in.failed = true;
        in.ended = true;
    } else {
        word.assign(buffer, n);
    }
    return in;
}

istream &getline(istream &in, string &line, char delimiter) {
    line.clear();
    while (1) {
        int c = in.get();
        if (c < 0) {
            in.ended = true;
            if (line.empty()) in.failed = true; else in.failed = false;
            break;
        }
        if (c == delimiter) break;
        line.push_back((char)c);
    }
    return in;
}

}
