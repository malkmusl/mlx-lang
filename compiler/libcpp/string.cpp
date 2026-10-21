// std::string (include/c++/string): a NUL-ended buffer of `room` bytes
// holding `count` chars, grown by doubling.
#include <string>
#include <utility>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace std {

static char *allocate(size_t bytes) {
    char *memory = (char *)malloc(bytes);
    if (!memory) {
        fputs("std::string: out of memory\n", stderr);
        abort();
    }
    return memory;
}

string::string() : chars(allocate(16)), count(0), room(16) { chars[0] = 0; }

string::string(const char *text) : chars(0), count(0), room(0) {
    size_t n = strlen(text);
    chars = allocate(n + 1);
    room = n + 1;
    memcpy(chars, text, n + 1);
    count = n;
}

string::string(const char *text, size_t n) : chars(allocate(n + 1)), count(n), room(n + 1) {
    memcpy(chars, text, n);
    chars[n] = 0;
}

string::string(size_t n, char fill) : chars(allocate(n + 1)), count(n), room(n + 1) {
    memset(chars, fill, n);
    chars[n] = 0;
}

string::string(const string &other) : chars(allocate(other.count + 1)), count(other.count), room(other.count + 1) {
    memcpy(chars, other.chars, other.count + 1);
}

string::~string() { free(chars); }

void string::grow(size_t needed) {
    if (needed + 1 <= room) return;
    size_t wanted = room * 2;
    if (wanted < needed + 1) wanted = needed + 1;
    char *grown = allocate(wanted);
    memcpy(grown, chars, count + 1);
    free(chars);
    chars = grown;
    room = wanted;
}

string &string::operator=(const string &other) {
    if (this != &other) assign(other.chars, other.count);
    return *this;
}

string &string::operator=(const char *text) { return assign(text, strlen(text)); }

string &string::operator=(char c) { return assign(&c, 1); }

string &string::assign(const char *text, size_t n) {
    grow(n);
    memmove(chars, text, n);
    chars[n] = 0;
    count = n;
    return *this;
}

char &string::at(size_t index) {
    if (index >= count) {
        fputs("std::string::at: index out of range\n", stderr);
        abort();
    }
    return chars[index];
}

void string::clear() {
    count = 0;
    chars[0] = 0;
}

void string::reserve(size_t wanted) { grow(wanted); }

void string::resize(size_t wanted, char fill) {
    if (wanted > count) {
        grow(wanted);
        memset(chars + count, fill, wanted - count);
    }
    count = wanted;
    chars[count] = 0;
}

void string::push_back(char c) {
    grow(count + 1);
    chars[count++] = c;
    chars[count] = 0;
}

void string::pop_back() {
    if (count > 0) chars[--count] = 0;
}

string &string::append(const char *text, size_t n) {
    grow(count + n);
    memcpy(chars + count, text, n);
    count += n;
    chars[count] = 0;
    return *this;
}

string &string::append(const char *text) { return append(text, strlen(text)); }

string &string::append(const string &other) { return append(other.chars, other.count); }

string &string::append(size_t n, char c) {
    grow(count + n);
    memset(chars + count, c, n);
    count += n;
    chars[count] = 0;
    return *this;
}

string &string::insert(size_t position, const char *text) {
    size_t n = strlen(text);
    if (position > count) position = count;
    grow(count + n);
    memmove(chars + position + n, chars + position, count - position + 1);
    memcpy(chars + position, text, n);
    count += n;
    return *this;
}

string &string::insert(size_t position, const string &other) {
    string copy(other);
    return insert(position, copy.chars);
}

string &string::erase(size_t position, size_t n) {
    if (position > count) position = count;
    if (n == npos || position + n > count) n = count - position;
    memmove(chars + position, chars + position + n, count - position - n + 1);
    count -= n;
    return *this;
}

string &string::replace(size_t position, size_t n, const char *text) {
    erase(position, n);
    return insert(position, text);
}

string &string::replace(size_t position, size_t n, const string &other) {
    string copy(other);
    return replace(position, n, copy.chars);
}

string string::substr(size_t position, size_t n) const {
    if (position > count) position = count;
    if (n == npos || position + n > count) n = count - position;
    string part(chars + position, n);
    return part;
}

size_t string::find(const char *text, size_t from) const {
    size_t n = strlen(text);
    if (from > count) return npos;
    for (size_t i = from; i + n <= count; i++)
        if (memcmp(chars + i, text, n) == 0) return i;
    return npos;
}

size_t string::find(const string &other, size_t from) const { return find(other.chars, from); }

size_t string::find(char c, size_t from) const {
    for (size_t i = from; i < count; i++)
        if (chars[i] == c) return i;
    return npos;
}

size_t string::rfind(char c, size_t from) const {
    if (count == 0) return npos;
    size_t i = from == npos || from >= count ? count - 1 : from;
    while (1) {
        if (chars[i] == c) return i;
        if (i == 0) return npos;
        i--;
    }
}

size_t string::rfind(const char *text, size_t from) const {
    size_t n = strlen(text);
    if (n > count) return npos;
    size_t i = from == npos || from > count - n ? count - n : from;
    while (1) {
        if (memcmp(chars + i, text, n) == 0) return i;
        if (i == 0) return npos;
        i--;
    }
}

size_t string::find_first_of(const char *set, size_t from) const {
    for (size_t i = from; i < count; i++)
        if (strchr(set, chars[i])) return i;
    return npos;
}

size_t string::find_first_not_of(const char *set, size_t from) const {
    for (size_t i = from; i < count; i++)
        if (!strchr(set, chars[i])) return i;
    return npos;
}

size_t string::find_last_of(const char *set, size_t from) const {
    if (count == 0) return npos;
    size_t i = from == npos || from >= count ? count - 1 : from;
    while (1) {
        if (strchr(set, chars[i])) return i;
        if (i == 0) return npos;
        i--;
    }
}

size_t string::find_last_not_of(const char *set, size_t from) const {
    if (count == 0) return npos;
    size_t i = from == npos || from >= count ? count - 1 : from;
    while (1) {
        if (!strchr(set, chars[i])) return i;
        if (i == 0) return npos;
        i--;
    }
}

int string::compare(const char *text) const { return strcmp(chars, text); }

int string::compare(const string &other) const {
    size_t n = count < other.count ? count : other.count;
    int order = memcmp(chars, other.chars, n);
    if (order != 0) return order;
    if (count < other.count) return -1;
    if (count > other.count) return 1;
    return 0;
}

void string::swap(string &other) {
    char *c = chars; chars = other.chars; other.chars = c;
    size_t n = count; count = other.count; other.count = n;
    size_t r = room; room = other.room; other.room = r;
}

string operator+(const string &a, const string &b) {
    string result(a);
    result.append(b);
    return result;
}

string operator+(const string &a, const char *b) {
    string result(a);
    result.append(b);
    return result;
}

string operator+(const char *a, const string &b) {
    string result(a);
    result.append(b);
    return result;
}

string operator+(const string &a, char b) {
    string result(a);
    result.push_back(b);
    return result;
}

string operator+(char a, const string &b) {
    string result(1, a);
    result.append(b);
    return result;
}

bool operator==(const string &a, const string &b) { return a.compare(b) == 0; }
bool operator==(const string &a, const char *b) { return a.compare(b) == 0; }
bool operator==(const char *a, const string &b) { return b.compare(a) == 0; }
bool operator!=(const string &a, const string &b) { return a.compare(b) != 0; }
bool operator!=(const string &a, const char *b) { return a.compare(b) != 0; }
bool operator!=(const char *a, const string &b) { return b.compare(a) != 0; }
bool operator<(const string &a, const string &b) { return a.compare(b) < 0; }
bool operator<=(const string &a, const string &b) { return a.compare(b) <= 0; }
bool operator>(const string &a, const string &b) { return a.compare(b) > 0; }
bool operator>=(const string &a, const string &b) { return a.compare(b) >= 0; }

static string formatted(const char *format, double value) {
    char buffer[64];
    snprintf(buffer, sizeof buffer, format, value);
    string result(buffer);
    return result;
}

string to_string(int value) {
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%d", value);
    string result(buffer);
    return result;
}

string to_string(unsigned value) {
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%u", value);
    string result(buffer);
    return result;
}

string to_string(long value) {
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%ld", value);
    string result(buffer);
    return result;
}

string to_string(unsigned long value) {
    char buffer[32];
    snprintf(buffer, sizeof buffer, "%lu", value);
    string result(buffer);
    return result;
}

string to_string(double value) { return formatted("%f", value); }
string to_string(float value) { return formatted("%f", (double)value); }

static void checkNumber(const char *start, const char *stop, size_t *end) {
    if (stop == start) {
        fputs("std::stoi: no number\n", stderr);
        abort();
    }
    if (end) *end = (size_t)(stop - start);
}

int stoi(const string &text, size_t *end, int base) {
    char *stop;
    long value = strtol(text.c_str(), &stop, base);
    checkNumber(text.c_str(), stop, end);
    return (int)value;
}

long stol(const string &text, size_t *end, int base) {
    char *stop;
    long value = strtol(text.c_str(), &stop, base);
    checkNumber(text.c_str(), stop, end);
    return value;
}

unsigned long stoul(const string &text, size_t *end, int base) {
    char *stop;
    unsigned long value = strtoul(text.c_str(), &stop, base);
    checkNumber(text.c_str(), stop, end);
    return value;
}

double stod(const string &text, size_t *end) {
    char *stop;
    double value = strtod(text.c_str(), &stop);
    checkNumber(text.c_str(), stop, end);
    return value;
}

float stof(const string &text, size_t *end) { return (float)stod(text, end); }

}
