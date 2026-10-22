// An explicit specialization of one of two function templates of a
// name, chosen by its parameters with the arguments given bound first:
// `writeLittleEndian<uint8_t>(vector<uint8_t> &, span<const uint8_t>)` is
// the span template's, not that of `(vector<uint8_t> &, T)` with T
// uint8_t already (JavaScriptCore's CloneSerializerBase.h: "the
// instance exists already").
#include <cstdio>
#include <span>
#include <vector>
#include <cstdint>
template<typename T> inline void writeLittleEndian(std::vector<uint8_t>& buffer, T value)
{
    for (unsigned i = 0; i < sizeof(T); i++) {
        buffer.push_back(value & 0xFF);
        value >>= 8;
    }
}
template<> inline void writeLittleEndian<uint8_t>(std::vector<uint8_t>& buffer, uint8_t value)
{
    buffer.push_back(value);
    buffer.push_back(0xEE);
}
template<typename T> inline bool writeLittleEndian(std::vector<uint8_t>& buffer, std::span<const T> values)
{
    for (unsigned i = 0; i < values.size(); i++)
        writeLittleEndian(buffer, values[i]);
    return true;
}
template<> inline bool writeLittleEndian<uint8_t>(std::vector<uint8_t>& buffer, std::span<const uint8_t> values)
{
    buffer.push_back(0xAA);
    buffer.insert(buffer.end(), values.begin(), values.end());
    return true;
}
int main()
{
    std::vector<uint8_t> buffer;
    writeLittleEndian(buffer, (uint16_t)0x1234);
    writeLittleEndian(buffer, (uint8_t)7);
    uint8_t bytes[2] = { 1, 2 };
    uint16_t words[1] = { 0x5678 };
    writeLittleEndian(buffer, std::span<const uint8_t>(bytes));
    writeLittleEndian(buffer, std::span<const uint16_t>(words));
    for (auto b : buffer) printf("%02x ", b);
    printf("\n");
    return 3;
}
