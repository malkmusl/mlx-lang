// A unique_ptr<const char[]> made from a unique_ptr<char[]> (Skia's
// SkRescaleAndReadPixels: `std::make_unique<Result>(std::move(data), ...)`
// of a local class taking the const one).
#include <cstdio>
#include <cstring>
#include <memory>

struct ReadResult {
    virtual ~ReadResult() {}
    virtual const void *data() const = 0;
};

static size_t use(std::unique_ptr<ReadResult> result) { return strlen((const char *)result->data()); }

static size_t run() {
    std::unique_ptr<char[]> data(new char[8]);
    strcpy(data.get(), "pixels");
    class Result : public ReadResult {
    public:
        Result(std::unique_ptr<const char[]> data, size_t rowBytes) : fData(std::move(data)), fRowBytes(rowBytes) {}
        const void *data() const override { return fData.get(); }
    private:
        std::unique_ptr<const char[]> fData;
        size_t fRowBytes;
    };
    return use(std::make_unique<Result>(std::move(data), 8));
}

int main(int argc, char **argv) {
    std::unique_ptr<int[]> ints(new int[2]);
    ints[0] = 4;
    ints[1] = 5;
    std::unique_ptr<const int[]> constInts;
    constInts = std::move(ints);
    printf("%zu %d %d\n", run(), constInts[1], ints.get() == nullptr);
    return argc;
}
