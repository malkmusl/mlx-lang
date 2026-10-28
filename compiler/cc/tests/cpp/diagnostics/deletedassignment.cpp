// An object assigned from an lvalue of a class whose copy assignment is
// deleted (a unique_ptr copied by an algorithm): an error naming the
// class, not the implicit assignment's bytes.
struct Owner {
    int* pointer = nullptr;
    Owner() = default;
    Owner(Owner&&) = default;
    Owner& operator=(Owner&&) = default;
    Owner& operator=(const Owner&) = delete;
};
void copy(Owner& to, Owner& from)
{
    to = from;
}
