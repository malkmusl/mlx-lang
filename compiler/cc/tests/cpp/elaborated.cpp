// An elaborated type specifier naming no class yet in a member's
// declaration (`static bool IsValid(const struct SkPictInfo &)` in
// SkPicture) declares the class in the namespace around: defined later
// there, it is the one the member functions see.
#include <cstdio>

namespace sk {
class Picture {
public:
    static bool IsValid(const struct PictInfo &info);
    struct PictInfo createHeader() const;
};

struct PictInfo {
    int fVersion;
    char fMagic[4];
};

bool Picture::IsValid(const PictInfo &info) { return info.fVersion > 2; }
PictInfo Picture::createHeader() const {
    PictInfo info;
    info.fVersion = 7;
    info.fMagic[0] = 's';
    return info;
}
}

int main(int argc, char **argv) {
    sk::Picture p;
    sk::PictInfo info = p.createHeader();
    printf("%d %d %c\n", sk::Picture::IsValid(info), info.fVersion, info.fMagic[0]);
    return argc;
}
