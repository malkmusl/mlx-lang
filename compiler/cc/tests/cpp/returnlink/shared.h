// Classes returned by value between code g++ compiled and ours (the
// Itanium ABI): one of 16 bytes that is not trivially copyable (pointing
// into itself) in memory, a trivially copyable one of 16 bytes in
// registers.
struct Self {
    Self(int v);
    Self(const Self& o);
    ~Self();
    int value() const { return *data; }
    bool home() const { return data == &inlineValue; }
    int* data;
    int inlineValue;
};
struct Pair {
    long a;
    long b;
};
Self gppSelf(int v);
Pair gppPair(long a);
Self mlxSelf(int v);
Pair mlxPair(long a);
