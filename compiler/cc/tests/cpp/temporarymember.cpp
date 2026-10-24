// `Iterator it(View(string).left(2));`: an object made with a temporary
// that a member call uses is an initializer, not a parameter named
// string (WebCore's StringTruncator: `NonSharedCharacterBreakIterator
// it(StringView(string).left(length));`).
#include <cstdio>

struct View {
    const char* text;
    View(const char* t) : text(t) { }
    View left(int n) const { return View(text + n); }
};

struct Iterator {
    const char* at;
    Iterator(View v) : at(v.text) { }
};

int main()
{
    const char* string = "hello";
    Iterator it(View(string).left(2));
    printf("%s\n", it.at);
    View* view = new View(string);
    Iterator other(View(view->text).left(1).left(1));
    printf("%s\n", other.at);
    delete view;
    return 0;
}
