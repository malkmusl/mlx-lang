// std::is_convertible of a pointer to a class template's instance named
// only so far (`Pool<WorkList> *` to `Executor *`): the instance is made
// whole, its bases decide. Skia's SkExecutor::MakeFIFOThreadPool returns
// std::make_unique<SkThreadPool<WorkList>>(...) as an
// std::unique_ptr<SkExecutor>, through unique_ptr's converting constructor
// `requires is_convertible_v<U *, T *>`.
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <type_traits>

class Executor {
public:
    virtual ~Executor() = default;
    virtual int work() = 0;
    static std::unique_ptr<Executor> make(int threads);
};

template<typename WorkList> class Pool final : public Executor {
public:
    Pool(int lists, int threads, bool borrow) : fLists(lists), fThreads(threads), fBorrow(borrow) {}
    int work() override { return fLists * 100 + fThreads * 10 + fBorrow; }
private:
    std::unique_ptr<WorkList[]> fWork;
    int fLists;
    int fThreads;
    const bool fBorrow;
};

std::unique_ptr<Executor> Executor::make(int threads)
{
    using WorkList = std::deque<std::function<void(void)>>;
    return std::make_unique<Pool<WorkList>>(/* lists */ 1, threads > 0 ? threads : 4, true);
}

template<typename W> struct Simple final : Executor { int work() override { return 7; } };

int main()
{
    printf("%d %d %d\n", (int)std::is_convertible_v<Pool<int>*, Executor*>, (int)std::is_convertible_v<Simple<std::deque<int>>*, Executor*>, (int)std::is_convertible_v<Executor*, Simple<int>*>);
    auto e = Executor::make(2);
    std::unique_ptr<Executor> s = std::make_unique<Simple<long>>();
    printf("%d %d\n", e->work(), s->work());
    return 3;
}
