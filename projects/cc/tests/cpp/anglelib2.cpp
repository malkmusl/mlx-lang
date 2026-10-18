// The library ANGLE takes, 2: <future> (promise, future, async),
// <fstream> (ofstream written and ifstream read back, seekg and tellg,
// istreambuf_iterator), istringstream reading in the base its flags
// say, constexpr std::pair, the si_code constants of <signal.h>.
#include <stdio.h>
#include <signal.h>
#include <future>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <iterator>

static int square(int x) { return x * x; }

template <typename T> static bool lexInt(const std::string &text, std::ios::fmtflags base, T *value) {
    std::istringstream stream(text);
    stream.setf(base, std::ios::basefield);
    stream >> (*value);
    return !stream.fail();
}

constexpr std::pair<int, char> kPair(3, 'x');
static_assert(kPair.first == 3);

int main() {
    std::promise<int> promise;
    std::future<int> result = promise.get_future();
    std::thread worker([&promise]() { promise.set_value(42); });
    worker.join();
    std::future<int> later = std::async(std::launch::deferred, square, 9);
    std::future<int> now = std::async(square, 5);
    int got = result.get();
    printf("future %d %d %d %d\n", got, later.get(), now.get(), (int)result.valid());

    {
        std::ofstream out("anglelib2.txt");
        out << "line one\n" << 12 << ' ' << 3.5 << "\nlast";
    }
    std::ifstream in("anglelib2.txt", std::ios::binary);
    in.seekg(0, std::ios::end);
    long size = (long)in.tellg();
    in.seekg(0, std::ios::beg);
    std::string first;
    std::getline(in, first);
    int n = 0;
    double d = 0;
    in >> n >> d;
    in.close();
    std::ifstream again("anglelib2.txt");
    std::string whole((std::istreambuf_iterator<char>(again)), std::istreambuf_iterator<char>());
    std::ifstream missing("anglelib2-none.txt");
    printf("fstream %ld [%s] %d %g %zu %d %d\n", size, first.c_str(), n, d, whole.size(), (int)missing.fail(), (int)again.is_open());
    remove("anglelib2.txt");

    unsigned u = 0;
    int i = 0;
    bool hex = lexInt("ff", std::ios::hex, &u);
    unsigned hexValue = u;
    bool oct = lexInt("17", std::ios::oct, &u);
    bool big = lexInt("99999999999", std::ios::dec, &i);
    bool bad = lexInt("12z", std::ios::dec, &i);
    printf("lex %d %u %d %u %d %d\n", (int)hex, hexValue, (int)oct, u, (int)big, (int)bad);
    printf("pair %d %c signal %d %d\n", kPair.first, kPair.second, SEGV_ACCERR, BUS_ADRERR);
    return 0;
}
