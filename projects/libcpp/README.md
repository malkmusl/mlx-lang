# mlxlibcpp

The C++ standard library for mlx-cc (`projects/cc`, "C++"), written in
the C++ mlx-cc reads, over the C library (mlxlibc on MLX/Linux, glibc
elsewhere). It is small and grows with the compiler: what is here is
what programs use first.

    tools/build_mlxlibcpp.sh                  # mlx-out/libcpp/libmlxcpp.a
    MLX_CC_LIB=mlx-out/libcpp mlx-cc -o hello hello.cpp

mlx-cc links `libmlxcpp.a` into every C++ program by itself (from
`$MLX_CC_LIB`, from `lib/` next to the compiler, or from
`/usr/local/lib/mlx-cc/lib`; `-nostdlib++` leaves it out), and the math
library with it. Objects linked on their own are C++ with `-x c++`
(`mlx-cc -x c++ -o prog a.o b.o`) or with `-lmlxcpp`, as `g++` is the
driver for linking C++ objects. The headers are the compiler's, under
`projects/cc/include/c++`.

## What is there

- `<new>` and `new.cpp`: `operator new`, `new[]`, `delete`, `delete[]`
  over malloc and free (a program defining its own wins: the archive's
  are taken only when needed), and placement new (built into mlx-cc).
- `<string>` and `string.cpp`: `std::string` (a NUL-ended buffer grown by
  doubling): construction from C strings, counts and copies, `=`, `+=`,
  `+`, the comparisons, `[]`, `at`, `front`, `back`, `begin`/`end` as
  `char *`, `size`, `empty`, `c_str`, `data`, `push_back`, `pop_back`,
  `append`, `assign`, `insert`, `erase`, `replace`, `substr`, `find`,
  `rfind`, `find_first_of`, `find_first_not_of`, `find_last_of`,
  `find_last_not_of`, `compare`, `swap`, `clear`, `reserve`, `resize`,
  `npos`; `to_string` for the integer and floating types, `stoi`, `stol`,
  `stoul`, `stod`, `stof`.
- `<iostream>` (`<ostream>`, `<istream>`) and `iostream.cpp`: `cout`,
  `cerr`, `clog` with `<<` for the built-in types, C strings and
  `std::string` (doubles as `%g` with `precision()`), `endl`, `flush`,
  `ends`, `write`, `put`; `cin` with `>>` for the built-in types, C
  strings and `std::string` (a whitespace-separated word), `get`,
  `peek`, `ignore`, `getline` into a buffer, `std::getline` into a
  string, `eof`, `fail`, `good`, `clear`. The stream objects are plain
  aggregates over stdio's streams, so a program's own globals may use
  them in their constructors.
- `<vector>`: `std::vector<T>` (a doubling array; pointers as iterators)
  with `push_back`, `pop_back`, `[]`, `at`, `front`, `back`, `begin`,
  `end`, `data`, `size`, `capacity`, `empty`, `reserve`, `resize`,
  `assign`, `insert`, `erase`, `clear`, `swap`, `==`, `!=`; elements are
  constructed and destroyed as C++ asks, through placement new.
- `<utility>`: `std::pair`, `make_pair`, the comparisons, `std::swap`.
- `<algorithm>`: `min`, `max`, `fill`, `count`, `find`, `reverse`,
  `copy`, `equal`, `sort`, `unique`; `<numeric>`: `accumulate`. They
  take pointers (a vector's, a string's, an array's) as iterators.
- The C headers in `std`: `<cstdio>`, `<cstdlib>` (with `std::abs` for
  int, long, and in `<cmath>` double and float), `<cstring>`,
  `<cctype>`, `<cmath>`, `<cstddef>`, `<cstdint>`, `<cstdarg>`,
  `<climits>`, `<cfloat>`, `<cassert>`, `<cerrno>`, `<ctime>`.

## Not there

`<map>`, `<set>`, `<list>`, `<memory>`, `<functional>`, iterators that
are not pointers, lambdas as predicates (`sort` with a comparison);
exceptions (`std::string::at` out of range aborts); conversion to bool of
a stream (`while (cin >> x)`: test `cin.fail()` instead); formatting
flags beyond the precision (`setw`, `hex`, `fixed`); file and string
streams; wide strings; `std::abs(long long)` and the other `long long`
overloads (`long long` is `long` to mlx-cc).

## Check

`tools/check_cpp.sh` builds the library and runs `projects/cc/tests/cpp`
(`stdlib.cpp` uses it, reading `stdlib.stdin`; `containers.cpp` its
templates) against the output of g++'s builds, on glibc and on mlxlibc.
