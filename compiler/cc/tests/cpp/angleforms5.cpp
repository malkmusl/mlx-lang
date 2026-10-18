// ANGLE forms 5: an operator<< of a base class declared in a namespace's
// unnamed namespace, called from the namespace's own operator<< of the
// derived class (angletypes.cpp), and a string stream's buffer asked
// what can be read of it (InfoLog::empty in Program.cpp).
#include <stdio.h>
#include <iostream>
#include <memory>
#include <sstream>

namespace gl {
struct PixelStoreStateBase {
    int alignment = 4;
    int rowLength = 0;
};
struct PixelUnpackState : PixelStoreStateBase {
    int imageHeight = 7;
};
std::ostream &operator<<(std::ostream &os, const PixelUnpackState &unpackState);

namespace {
std::ostream &operator<<(std::ostream &os, const PixelStoreStateBase &state)
{
    os << "alignment = " << state.alignment << ", rowLength = " << state.rowLength;
    return os;
}
}  // namespace

std::ostream &operator<<(std::ostream &os, const PixelUnpackState &unpackState)
{
    os << static_cast<const PixelStoreStateBase &>(unpackState) << ", imageHeight = " << unpackState.imageHeight;
    return os;
}

class InfoLog {
public:
    bool empty() const
    {
        if (!mLazyStream) return true;
        return mLazyStream->rdbuf()->in_avail() == 0;
    }
    std::stringstream &stream()
    {
        if (!mLazyStream) mLazyStream.reset(new std::stringstream());
        return *mLazyStream;
    }
private:
    std::unique_ptr<std::stringstream> mLazyStream;
};
}  // namespace gl

int main() {
    gl::PixelUnpackState state;
    state.rowLength = 16;
    std::ostringstream out;
    out << state;
    printf("%s\n", out.str().c_str());
    gl::InfoLog log;
    printf("%d", log.empty());
    log.stream();
    printf(" %d", log.empty());
    log.stream() << "error: " << 3;
    printf(" %d\n", log.empty());
    return 0;
}
