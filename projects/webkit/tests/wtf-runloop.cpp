// The generic run loop's timers (USE_GENERIC_EVENT_LOOP, JSCOnly's WTF,
// port/wtf/RunLoopGenericMlx.cpp): a timer stopped while its task had
// children in the run loop's tree, one of them then destroyed, and the
// first started again (it released the dead child: WebCore's main
// thread stopped on it one run in 30 of crypto's RSA key generation);
// then timers started, stopped, destroyed and fired in a pseudo-random
// order, the run loop cycled between, each firing counted.
// Built by ./mlx build webkit-wtf-runloop; tools/check_jsc_mlxlibc.sh runs
// it and compares what it prints with tests/wtf-runloop.expected.
#include "config.h"

#include <stdio.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

static unsigned fired;

static std::unique_ptr<RunLoop::Timer> makeTimer()
{
    return makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), "wtf-runloop"_s, [] { fired++; });
}

int main()
{
    WTF::initializeMainThread();

    // The tree's root removed with two children, the left one destroyed,
    // the root inserted again.
    auto a = makeTimer();
    auto b = makeTimer();
    auto c = makeTimer();
    a->startOneShot(100_s);
    b->startOneShot(50_s);
    c->startOneShot(200_s);
    a->stop();
    b = nullptr;
    a->startOneShot(100_s);
    a = nullptr;
    c = nullptr;
    printf("a removed node's dead child: ok\n");

    // Many timers, in a pseudo-random order (a linear congruential
    // generator: the same order each run).
    uint32_t state = 12345;
    auto next = [&] {
        state = state * 1103515245 + 12345;
        return (state >> 16) & 0x7fff;
    };
    Vector<std::unique_ptr<RunLoop::Timer>> timers;
    for (unsigned i = 0; i < 64; ++i)
        timers.append(makeTimer());
    unsigned remade = 0;
    for (unsigned step = 0; step < 20000; ++step) {
        auto& timer = timers[next() % timers.size()];
        switch (next() % 5) {
        case 0:
        case 1:
            timer->startOneShot(Seconds::fromMilliseconds(next() % 3));
            break;
        case 2:
            timer->stop();
            break;
        case 3:
            timer = makeTimer();
            remade++;
            break;
        case 4:
            RunLoop::cycle();
            break;
        }
    }
    for (auto& timer : timers)
        timer->stop();
    printf("20000 steps: %s (%s timers made again, %s fired)\n", "ok", remade > 1000 ? "more than 1000" : "few", fired > 100 ? "more than 100" : "few");
    return 0;
}
