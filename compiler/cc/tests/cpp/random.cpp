// <random>: the engines' numbers as the standard fixes them (the
// 10000th of each), the distributions and shuffle drawing as libstdc++'s
// (WebCore's NavigatorUAData shuffles with a default_random_engine,
// AudioUtilities draws normal noise from an mt19937).
#include <algorithm>
#include <random>
#include <stdio.h>
#include <vector>

template <class E> unsigned long long tenThousandth(E e) {
    e.discard(9999);
    return e();
}

int main(int argc, char **)
{
    printf("%llu %llu\n", tenThousandth(std::minstd_rand0()), tenThousandth(std::minstd_rand()));
    printf("%llu %llu\n", tenThousandth(std::mt19937()), tenThousandth(std::mt19937_64()));
    std::mt19937 a(42), b(42);
    printf("%d %d\n", a == b, a() == b());
    a();
    printf("%d\n", a == b);

    std::mt19937 g(7);
    std::uniform_int_distribution<int> die(1, 6);
    for (int i = 0; i < 10; i++) printf("%d ", die(g));
    printf("\n");
    std::uniform_int_distribution<long long> wide(-1000000000000LL, 1000000000000LL);
    for (int i = 0; i < 3; i++) printf("%lld ", wide(g));
    printf("\n");
    std::mt19937_64 g64(3);
    std::uniform_int_distribution<unsigned> small(0, 99);
    for (int i = 0; i < 5; i++) printf("%u ", small(g64));
    printf("\n");
    std::minstd_rand0 lcg;
    std::uniform_int_distribution<unsigned long long> big;
    printf("%llu %d\n", big(lcg), die(lcg));

    std::uniform_real_distribution<double> unit;
    std::uniform_real_distribution<float> range(-2.0f, 3.0f);
    for (int i = 0; i < 3; i++) printf("%.6f %.4f ", unit(g), range(g));
    printf("\n");
    std::normal_distribution<float> noise(1, 0.5f);
    std::normal_distribution<double> standard;
    for (int i = 0; i < 4; i++) printf("%.5f %.5f ", noise(g), standard(g64));
    printf("\n");
    std::bernoulli_distribution coin(0.3);
    int heads = 0;
    for (int i = 0; i < 20; i++) heads += coin(g);
    std::exponential_distribution<double> wait(2.0);
    printf("%d %.5f\n", heads, wait(g));
    printf("%.8f\n", std::generate_canonical<double, 53>(g));

    std::vector<int> v { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    std::shuffle(v.begin(), v.end(), std::default_random_engine {});
    for (int x : v) printf("%d ", x);
    printf("\n");
    std::vector<int> w { 1, 2, 3, 4, 5 };
    auto rng = std::default_random_engine {};
    std::ranges::shuffle(w, rng);
    for (int x : w) printf("%d ", x);
    printf("\n");
    std::vector<int> many;
    for (int i = 0; i < 100; i++) many.push_back(i);
    std::shuffle(many.begin(), many.end(), g64);
    long sum = 0;
    for (int x : many) sum += x;
    printf("%d %d %d %ld\n", many[0], many[50], many[99], sum);

    std::seed_seq seq { 1, 2, 3 };
    std::mt19937 seeded(seq);
    std::minstd_rand lseeded(seq);
    printf("%u %u\n", (unsigned)seeded(), (unsigned)lseeded());
    unsigned words[4];
    seq.generate(words, words + 4);
    printf("%u %u %u %u\n", words[0], words[1], words[2], words[3]);

    std::random_device device;
    unsigned x = device();
    std::mt19937 fromDevice(device());
    (void)x;
    printf("%d\n", fromDevice.min() == 0 && fromDevice.max() == 0xffffffffu);
    return argc;
}
