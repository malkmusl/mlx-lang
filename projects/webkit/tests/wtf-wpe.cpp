// WTF of the WPE port at work: the main run loop (GLib's main loop under
// it, mlx-glib) with a repeating timer and a delayed dispatch, a work
// queue's thread dispatching back to the main thread, strings and URLs.
// Built by ./mlx build webkit-wpe-wtf-test; tools/check_wpe_wtf.sh runs it
// and compares what it prints with tests/wtf-wpe.expected.
#include "config.h"

#include <stdio.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

static void say(const String& text)
{
    printf("%s\n", text.utf8().data());
    fflush(stdout);
}

static String yesNo(bool value)
{
    return value ? "yes"_s : "no"_s;
}

static void strings()
{
    String version = makeString("WPE WebKit "_s, 2, '.', 54, '.', 1);
    say(makeString("string: "_s, version, " ("_s, version.length(), " characters)"_s));
    String greeting = String::fromUTF8("Gr\xc3\xbc\xc3\x9f" "e aus K\xc3\xb6ln");
    say(makeString("utf-8: "_s, greeting, " ("_s, greeting.length(), " characters, 8-bit: "_s, yesNo(greeting.is8Bit()), ')'));
    say(makeString("uppercase: "_s, version.convertToASCIIUppercase(), ", find \"Kit\": "_s, version.find("Kit"_s)));
    StringBuilder builder;
    for (auto& part : String("one,two,,three"_s).split(','))
        builder.append('[', part, ']');
    say(makeString("split: "_s, builder.toString()));
    say(makeString("number: "_s, String::number(3.25), ' ', String::number(-17), ' ', String::number(1e21)));
}

static void urls()
{
    URL url { "HTTPS://user:secret@Example.ORG:8080/a/./b/../c%20d?q=1&r=2#frag"_s };
    say(makeString("url: "_s, url.string()));
    say(makeString("  valid: "_s, yesNo(url.isValid()), ", protocol: "_s, url.protocol(), ", host: "_s, url.host(), ", port: "_s, url.port() ? *url.port() : 0));
    say(makeString("  user: "_s, url.user(), ", password: "_s, url.password(), ", path: "_s, url.path(), ", query: "_s, url.query(), ", fragment: "_s, url.fragmentIdentifier()));
    URL relative { url, "../x/y.html?z#top"_s };
    say(makeString("relative: "_s, relative.string()));
    URL file { "file:///tmp/some%20file.txt"_s };
    say(makeString("file: "_s, file.string(), ", path: "_s, file.fileSystemPath()));
    URL invalid { "http://[::1"_s };
    say(makeString("invalid: "_s, yesNo(!invalid.isValid())));
}

int main()
{
    WTF::initializeMainThread();
    strings();
    urls();

    auto& main = RunLoop::mainSingleton();
    say(makeString("main thread: "_s, yesNo(isMainThread()), ", main run loop current: "_s, yesNo(main.isCurrent())));

    auto queue = WorkQueue::create("wtf-test"_s);
    unsigned fired = 0;
    RunLoop::Timer timer(main, "wtf-test"_s, [&] {
        fired++;
        say(makeString("timer fired "_s, fired, " (main thread: "_s, yesNo(isMainThread()), ')'));
        if (fired < 3)
            return;
        timer.stop();
        say(makeString("timer active after stop: "_s, yesNo(timer.isActive())));
        // Work on the queue's thread, then back to the main one.
        queue->dispatch([&main] {
            uint64_t sum = 0;
            for (uint64_t i = 1; i <= 1000000; i++)
                sum += i;
            bool onMain = isMainThread();
            main.dispatch([sum, onMain, &main] {
                say(makeString("work queue: sum "_s, sum, " computed off the main thread: "_s, yesNo(!onMain)));
                say(makeString("back on the main thread: "_s, yesNo(isMainThread())));
                MonotonicTime start = MonotonicTime::now();
                main.dispatchAfter(50_ms, [start, &main] {
                    Seconds waited = MonotonicTime::now() - start;
                    say(makeString("dispatchAfter fired after at least 50 ms: "_s, yesNo(waited >= 49_ms)));
                    main.stop();
                });
            });
        });
    });
    timer.startRepeating(10_ms);
    say(makeString("timer active: "_s, yesNo(timer.isActive())));
    RunLoop::run();
    say("run loop stopped"_s);
    return 0;
}
