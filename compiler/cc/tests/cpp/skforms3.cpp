// Forms of Skia's: a base's nested class named in a nested class's
// member declaration (`Stats::ProgramCacheResult *` in GrGLGpu's
// ProgramCache), an aggregate with a reference member initialized from
// braces (SkShaper's RunInfo).
#include <cstdio>
#include <cstddef>
#include <cstdint>

class PipelineBuilder {
public:
    class Stats {
    public:
        enum class ProgramCacheResult { kHit, kMiss, kPartial, kLast = kPartial };
        int fHits = 0;
    };
    Stats *stats() { return &fStats; }
protected:
    Stats fStats;
};

class Gpu {
public:
    class ProgramCache : public PipelineBuilder {
    public:
        int findOrCreate(int key);
        int findOrCreate(int key, const char *name, Stats::ProgramCacheResult *);
    };
};

int Gpu::ProgramCache::findOrCreate(int key) {
    Stats::ProgramCacheResult result;
    const int made = findOrCreate(key, "k", &result);
    return made + (int)result;
}
int Gpu::ProgramCache::findOrCreate(int key, const char *, Stats::ProgramCacheResult *result) {
    *result = key > 1 ? Stats::ProgramCacheResult::kHit : Stats::ProgramCacheResult::kMiss;
    fStats.fHits += key;
    return key * 10;
}

struct Font { float size; float measure(const char *text, size_t n) const { return size * n; } };
struct Vector { float fX, fY; };
struct Range {
    Range(size_t start, size_t size) : fStart(start), fSize(size) {}
    size_t fStart, fSize;
};
struct RunInfo {
    const Font &fFont;
    uint8_t fBidiLevel;
    uint32_t fScript;
    const char *fLanguage;
    Vector fAdvance;
    size_t glyphCount;
    Range utf8Range;
};

int main(int argc, char **argv) {
    Gpu::ProgramCache cache;
    const int found = cache.findOrCreate(3);
    Font font{2};
    const char *utf8 = "abcd";
    size_t numGlyphs = 4;
    float tracking = 0.5f;
    const RunInfo info = {
        font, 0, 0, "",
        { font.measure(utf8, 4) + numGlyphs * tracking, 0 },
        numGlyphs,
        Range(1, 3)
    };
    printf("%d %d %.1f %zu %zu %.0f\n", found, cache.stats()->fHits, info.fAdvance.fX, info.glyphCount, info.utf8Range.fSize, info.fFont.size);
    return argc;
}
