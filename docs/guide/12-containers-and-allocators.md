# Containers and allocators

**Source tests:** `390_heap_runtime.mlx`, `386_hash_runtime.mlx`,
`387_hash_map_runtime.mlx`, `388_array_list_runtime.mlx`,
`389_hash_set_runtime.mlx` (the std module tests,
`tests/run_std_modules.sh`; 387 to 389 run once the compiler takes a
struct type in a function body)

A program has one heap and passes it to what needs memory; what needs
memory is a container of the standard library, which gives the memory
back by itself where it goes out of scope. That is the whole idiom.

## One heap per program

`std.heap.allocator()` is the program's heap: a general-purpose
allocator (size classes, free lists) over the page allocator, made on
first use and the same wherever it is asked for, in every image of the
process. It is the allocator a program takes and hands on; the page
allocator itself, `std.page_allocator.init()`, is for mappings and for
the backing of other allocators (`mlx-codemap workarounds` lists the
places that take it otherwise).

```mlx
const allocator = heap.allocator()
const again = heap.allocator()
if @intFromPtr(allocator.context) != @intFromPtr(again.context) { return 1 }
const small = allocator.alloc(100, 8)
if small == null { return 4 }
if heap.liveBytes() != before + 100 { return 5 }
```

(`tests/390_heap_runtime.mlx`)

Its blocks come zeroed, as the page allocator's do, and under
`MLX_LEAK_CHECK=1` every block it hands out is seen by `std.leaks` with
the function that asked for it and the size class it took, which
`mlx-codemap memory PROGRAM` shows.

A phase that allocates much and frees nothing at all (a compiler's unit,
an index, one request) takes an arena, `std.arena_allocator`, over the
heap or, for a reservation of gigabytes, over the page allocator; its
`deinit` gives everything back at once.

## Containers that free themselves

The containers are generic over their types and `@noncopy(struct)` with
a `deinit`: a local one is dropped where its scope ends, a stored one
when its owner is ([Ownership](06-ownership.md)), so the memory a map or
a list holds goes back without a line written for it.

```mlx
var names = std.StringHashMap(u32).init(allocator)
if !names.put("one", 1) || !names.put("two", 2) || !names.put("three", 3) { return 2 }
const two = names.get("two")
if two == null || two.? != 2 { return 4 }
const pointer = names.getPtr("three")
pointer.?.* = 33
var entries = names.iterator()
while true {
    const entry = entries.next()
    if entry == null { break }
    sum += entry.?.value.*
}
```

(`tests/387_hash_map_runtime.mlx`)

A `std.StringHashMap(V)` hashes string keys by their content; a
`std.AutoHashMap(K, V)` derives the hash and the equality from the key
type by reflection (`std.hash.autoHash`, integers, enums, pointers,
optionals, slices, structs field by field); a `std.HashMap(K, V,
Context)` takes the caller's `hash` and `eql`, with state when the
context needs it; a `std.AutoHashSet(K)` holds keys alone.

```mlx
var numbers = std.ArrayList(u32).init(allocator)
if !numbers.append(1) || !numbers.append(2) || !numbers.append(3) { return 2 }
if sumOf(numbers.items()) != 6 { return 5 }
if !numbers.insert(0, 100) || numbers.items()[0] != 100 { return 8 }
if numbers.orderedRemove(0) != 100 { return 12 }
const popped = numbers.pop()
```

(`tests/388_array_list_runtime.mlx`)

A `std.ArrayList(T)` is a growing array, its elements the slice
`items()`. Out of memory is answered, not trapped: `put`, `append` and
their kin say `false`, `get` and `pop` say `null`.

## The rules

- Take `std.heap.allocator()` once, at the top of the program or the
  module that owns the data, and pass it: `init(allocator)`. A library
  does not fetch an allocator of its own behind the caller's back.
- A table is a `HashMap`; a growing array is an `ArrayList`. Hand-written
  tables (an open-addressing probe over arrays of a constant size) and
  arrays (a `[*]T` with a length and a `capacity` next to it) are what
  `mlx-codemap workarounds` reports as `tables` and `arrays`.
- A hash is `std.hash.wyhash` or `autoHash`; FNV written out is not.
- What must stay on raw pages: a mapping (a file, a frame buffer), the
  backing of an arena or a heap, and the leak checker's own tables.
