// `C(C &&) = default` and `C &operator=(C &&) = default` beside declared
// copies: an rvalue is moved (its storage kept), not copied (WTF's
// FixedVector of the DFG's switch tables, whose JIT code points into it).
#include <stdio.h>
#include <memory>
#include <utility>
struct Storage { int size; int data[4]; std::unique_ptr<Storage> clone() const { return std::make_unique<Storage>(*this); } };
template<typename T> class FixedVector {
public:
    FixedVector() = default;
    FixedVector(const FixedVector& other) : m_storage(other.m_storage ? other.m_storage->clone() : nullptr) { }
    FixedVector(FixedVector&&) = default;
    FixedVector& operator=(const FixedVector& other)
    {
        FixedVector tmp(other);
        swap(tmp);
        return *this;
    }
    FixedVector& operator=(FixedVector&&) = default;
    explicit FixedVector(int size) : m_storage(std::make_unique<Storage>()) { m_storage->size = size; }
    void swap(FixedVector& other) { std::swap(m_storage, other.m_storage); }
    int* data() { return m_storage ? m_storage->data : nullptr; }
    std::unique_ptr<Storage> m_storage;
};
struct Table { FixedVector<int> offsets; int def; };
int main()
{
    FixedVector<Table> graphTables(1);
    int* before = graphTables.data();
    FixedVector<Table> jitTables;
    jitTables = std::move(graphTables);
    FixedVector<Table> moved(std::move(jitTables));
    printf("%d %d %d %d\n", moved.data() == before, graphTables.data() == nullptr, jitTables.data() == nullptr, moved.m_storage->size);
    return 0;
}
