// <filesystem>: path decomposition and normalization, the operations
// (directories, files, links, copies, times, space) and the directory
// iterators, in the working directory's fsdir.
#include <stdio.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
namespace fs = std::filesystem;
static void show(const char *what, const fs::path &p) { printf("%s [%s]\n", what, p.c_str()); }
int main() {
    fs::path p("/usr/lib/libfoo.so.1");
    show("filename", p.filename());
    show("stem", p.stem());
    show("extension", p.extension());
    show("parent", p.parent_path());
    show("parent2", fs::path("/a/b/").parent_path());
    show("parent3", fs::path("/a").parent_path());
    show("parent4", fs::path("a").parent_path());
    show("filename2", fs::path("/a/b/").filename());
    show("rel", p.relative_path());
    show("root", p.root_path());
    show("join", fs::path("a") / "b" / "c.txt");
    show("join-abs", fs::path("a") / "/b");
    const char *norms[] = { "a/./b/../c", "/../x", "a/b/..", "a/.", "../../a", "//a//b//", "", "a/../..", "./" };
    for (const char *n : norms) show(n, fs::path(n).lexically_normal());
    show("relative", fs::path("/a/b/c").lexically_relative("/a/d"));
    fs::path q("dir/file.tar.gz");
    q.replace_extension(".xz"); show("replace_ext", q);
    q.replace_filename("other"); show("replace_fn", q);
    printf("eq %d abs %d %d\n", (int)(fs::path("x") == "x"), (int)p.is_absolute(), (int)fs::path("x").is_absolute());
    std::string s = p.filename(); printf("string %s %c\n", s.c_str(), fs::path::preferred_separator);

    std::error_code ec;
    fs::path base = fs::path("fsdir");
    fs::remove_all(base, ec);
    bool made = fs::create_directories(base / "x/y/z", ec); printf("create %d %d\n", (int)made, (int)(bool)ec);
    made = fs::create_directories(base / "x/y/z", ec); printf("again %d %d\n", (int)made, (int)(bool)ec);
    FILE *f = fopen((base / "x/a.txt").c_str(), "w"); fputs("hello world", f); fclose(f);
    printf("size %d exists %d %d\n", (int)fs::file_size(base / "x/a.txt", ec), (int)fs::exists(base / "x/a.txt", ec), (int)fs::exists(base / "nope", ec));
    printf("nope ec %d\n", (int)(bool)ec);
    fs::file_size(base / "nope", ec); printf("size-nope %d %s\n", (int)(ec == std::errc::no_such_file_or_directory), ec ? "err" : "ok");
    printf("copy %d\n", (int)fs::copy_file(base / "x/a.txt", base / "x/b.txt", ec));
    bool copied = fs::copy_file(base / "x/a.txt", base / "x/b.txt", ec); printf("copy-again %d %d\n", (int)copied, (int)(bool)ec);
    copied = fs::copy_file(base / "x/a.txt", base / "x/b.txt", fs::copy_options::overwrite_existing, ec); printf("copy-over %d %d\n", (int)copied, (int)(bool)ec);
    fs::create_symlink("a.txt", base / "x/link", ec);
    fs::create_hard_link(base / "x/a.txt", base / "x/hard", ec);
    printf("links %d %d %d\n", (int)fs::hard_link_count(base / "x/a.txt", ec), (int)(fs::symlink_status(base / "x/link", ec).type() == fs::file_type::symlink), (int)(fs::status(base / "x/link", ec).type() == fs::file_type::regular));
    show("read_symlink", fs::read_symlink(base / "x/link", ec));
    std::vector<std::string> names;
    auto entries = fs::directory_iterator(base / "x", ec);
    for (auto it = fs::begin(entries), end = fs::end(entries); !ec && it != end; it.increment(ec)) names.push_back(it->path().filename().string());
    std::sort(names.begin(), names.end());
    for (auto &n : names) printf("entry %s\n", n.c_str());
    std::vector<std::string> all;
    unsigned long long total = 0;
    for (auto &entry : fs::recursive_directory_iterator(base, ec)) {
        all.push_back(entry.path().lexically_relative(base).string());
        if (entry.is_regular_file(ec) && !ec) total += entry.file_size(ec);
    }
    std::sort(all.begin(), all.end());
    for (auto &n : all) printf("all %s\n", n.c_str());
    printf("total %llu\n", total);
    fs::copy(base / "x", base / "copy", fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
    printf("copied %d %d\n", (int)(bool)ec, (int)fs::exists(base / "copy/y/z"));
    fs::rename(base / "copy", base / "moved", ec);
    printf("moved %d %d\n", (int)fs::exists(base / "moved/a.txt"), (int)fs::exists(base / "copy"));
    auto t = fs::last_write_time(base / "x/a.txt", ec);
    fs::last_write_time(base / "x/b.txt", t, ec);
    printf("times %d %d\n", (int)(bool)ec, (int)(fs::last_write_time(base / "x/b.txt") == t));
    auto sp = fs::space(base, ec);
    printf("space %d %d\n", (int)(bool)ec, (int)(sp.capacity >= sp.available && sp.capacity > 0));
    printf("canonical %d\n", (int)(fs::canonical(base / "x/../x/a.txt", ec) == fs::canonical(base, ec) / "x/a.txt"));
    printf("temp %d\n", (int)!fs::temp_directory_path(ec).empty());
    bool removed = fs::remove(base / "x/b.txt", ec); bool again = fs::remove(base / "x/b.txt", ec); printf("remove %d %d\n", (int)removed, (int)again);
    printf("remove_all %d\n", (int)fs::remove_all(base, ec));
    printf("gone %d\n", (int)fs::exists(base));
    std::u8string u8(u8"café");
    fs::path up(u8);
    printf("u8 %d %s\n", (int)up.u8string().size(), up.c_str());
    return 0;
}
