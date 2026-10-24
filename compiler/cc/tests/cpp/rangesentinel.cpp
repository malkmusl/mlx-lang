// A range whose end() gives a sentinel of another type than begin()'s
// iterator, compared with it (WebCore's ElementChildRange: `static
// constexpr std::nullptr_t end()`, its iterator equal to nullptr when
// done). The iterator's class has no constructor from the sentinel.
#include <stddef.h>
#include <stdio.h>

struct Node { int value; Node *next; };

struct Iterator {
    Iterator(const Node *parent, Node *current) : parent(parent), current(current) { }
    Node &operator*() const { return *current; }
    Iterator &operator++() { current = current->next; return *this; }
    bool operator==(decltype(nullptr)) const { return !current; }
    const Node *parent;
    Node *current;
};

struct Range {
    Range(Node &parent) : parent(parent) { }
    Iterator begin() const { return Iterator(&parent, parent.next); }
    static constexpr decltype(nullptr) end() { return nullptr; }
    Node &parent;
};

struct Countdown { int left; };
struct CountdownIterator {
    int left;
    int operator*() const { return left; }
    CountdownIterator &operator++() { --left; return *this; }
    bool operator!=(Countdown) const { return left > 0; }
};
struct Counting {
    int from;
    CountdownIterator begin() const { return { from }; }
    Countdown end() const { return { 0 }; }
};

int main(int argc, char **argv)
{
    Node c { 3, nullptr };
    Node b { 2, &c };
    Node a { 1, &b };
    Node head { 0, &a };
    int sum = 0;
    for (Node &node : Range(head))
        sum = sum * 10 + node.value;
    printf("%d\n", sum);
    for (int n : Counting { argc + 1 })
        printf("%d ", n);
    printf("\n");
    return argc - 2;
}
