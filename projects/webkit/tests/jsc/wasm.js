// WebAssembly in jsc: modules assembled here byte by byte (no tools), run
// in the interpreter (IPInt), then hot enough for BBQ and OMG; traps
// (out-of-bounds memory through the fault handler, division by zero,
// unreachable, a bad indirect call, a NaN truncated), imports, tables,
// memory.grow, i64 and f64 arithmetic, SIMD (v128 lanes added and
// multiplied). What it prints is node's.
// (Each value printed as its String: a BigInt without node's `n`.)
const say = (...values) => (typeof print === "function" ? print : console.log)(values.map(String).join(" "));
function uleb(n) { const out = []; do { let b = n & 0x7f; n >>>= 7; if (n) b |= 0x80; out.push(b); } while (n); return out; }
function sleb(n) { n = BigInt(n); const out = []; for (;;) { const b = Number(n & 0x7fn); n >>= 7n; if ((n === 0n && !(b & 0x40)) || (n === -1n && (b & 0x40))) { out.push(b); return out; } out.push(b | 0x80); } }
function str(s) { return [...uleb(s.length), ...[...s].map(c => c.charCodeAt(0))]; }
function section(id, bytes) { return [id, ...uleb(bytes.length), ...bytes]; }
function vec(items) { return [...uleb(items.length), ...items.flat()]; }
const I32 = 0x7f, I64 = 0x7e, F64 = 0x7c;
// module({ types: [[params, results]], imports: [[mod, name, typeIndex]], funcs: [[typeIndex, locals, body]], exports: [[name, funcIndex]], memory: [min, max], table: n, elems: [indices] })
function module(m) {
    const out = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    out.push(...section(1, vec(m.types.map(([p, r]) => [0x60, ...vec(p.map(x => [x])), ...vec(r.map(x => [x]))]))));
    if (m.imports) out.push(...section(2, vec(m.imports.map(([mod, name, t]) => [...str(mod), ...str(name), 0x00, ...uleb(t)]))));
    out.push(...section(3, vec(m.funcs.map(f => uleb(f[0])))));
    if (m.table) out.push(...section(4, vec([[0x70, 0x00, ...uleb(m.table)]])));
    if (m.memory) out.push(...section(5, vec([[0x01, ...uleb(m.memory[0]), ...uleb(m.memory[1])]])));
    out.push(...section(7, vec(m.exports.map(([name, i, kind]) => [...str(name), kind ?? 0x00, ...uleb(i)]))));
    if (m.elems) out.push(...section(9, vec([[0x00, 0x41, 0x00, 0x0b, ...vec(m.elems.map(i => uleb(i)))]])));
    out.push(...section(10, vec(m.funcs.map(([, locals, body]) => { const b = [...vec(locals.map(([n, t]) => [...uleb(n), t])), ...body, 0x0b]; return [...uleb(b.length), ...b]; }))));
    return new Uint8Array(out);
}
const get = i => [0x20, ...uleb(i)], set = i => [0x21, ...uleb(i)], tee = i => [0x22, ...uleb(i)];
const i32 = n => [0x41, ...sleb(n)], i64 = n => [0x42, ...sleb(n)];
function instance(m, imports) { return new WebAssembly.Instance(new WebAssembly.Module(m), imports).exports; }
function trap(f) { try { f(); return "no trap"; } catch (e) { return e instanceof WebAssembly.RuntimeError ? "RuntimeError" : String(e); } }

// 1. i32 and i64 arithmetic, a loop, recursion.
const arith = instance(module({
    types: [[[I32, I32], [I32]], [[I64], [I64]], [[I32], [I32]], [[I64, I64], [I64]]],
    funcs: [
        [0, [], [...get(0), ...get(1), 0x6a]],                                     // add
        [1, [[1, I64]], [0x02, 0x40, 0x03, 0x40, ...get(0), 0x50, 0x0d, 0x01,        // sum n + (n-1) + ... (i64)
            ...get(1), ...get(0), 0x7c, ...set(1), ...get(0), ...i64(1), 0x7d, ...set(0), 0x0c, 0x00, 0x0b, 0x0b, ...get(1)]],
        [2, [], [...get(0), ...i32(2), 0x48, 0x04, I32, ...get(0), 0x05, ...get(0), ...i32(1), 0x6b, 0x10, 0x02, ...get(0), ...i32(2), 0x6b, 0x10, 0x02, 0x6a, 0x0b]], // fib
        [3, [], [...get(0), ...get(1), 0x7f, ...get(0), ...get(1), 0x81, 0x01, 0x7c, ...get(0), ...i64(13), 0x86, 0x01, 0x85, 0x01]], // (a/b + a%b) ^ (a<<13)
        [0, [], [...get(0), ...get(1), 0x6d]],                                     // div_s
    ],
    exports: [["add", 0], ["sum", 1], ["fib", 2], ["mix", 3], ["div", 4]],
}));
say(arith.add(2, 40), arith.add(0x7fffffff, 1), arith.sum(10n), arith.fib(20), arith.mix(123456789012345n, 97n));
let total = 0n; for (let i = 0; i < 20000; i++) total += arith.sum(BigInt(i % 300));
let adds = 0; for (let i = 0; i < 200000; i++) adds = arith.add(adds, i) | 0;
say(total, adds, arith.fib(25), trap(() => arith.div(1, 0)), trap(() => arith.div(-2147483648, -1)), arith.div(-7, 2));

// 2. Memory: f64 stores and loads, bounds checked by the fault handler, memory.grow.
const mem = instance(module({
    types: [[[I32, F64], []], [[I32], [F64]], [[I32], [I32]], [[], [I32]]],
    funcs: [
        [0, [], [...get(0), ...get(1), 0x39, 0x03, 0x00]],                         // f64.store
        [1, [], [...get(0), 0x2b, 0x03, 0x00]],                                    // f64.load
        [2, [], [...get(0), 0x40, 0x00]],                                          // memory.grow
        [3, [], [0x3f, 0x00]],                                                     // memory.size
        [2, [], [...get(0), 0x28, 0x02, 0x00]],                                    // i32.load
    ],
    memory: [1, 4],
    exports: [["store", 0], ["load", 1], ["grow", 2], ["size", 3], ["load32", 4], ["memory", 0, 0x02]],
}));
for (let i = 0; i < 1000; i++) mem.store(i * 8, Math.sqrt(i));
let squares = 0; for (let i = 0; i < 1000; i++) squares += mem.load(i * 8) ** 2;
say(squares.toFixed(6), mem.load(8 * 999).toFixed(10), trap(() => mem.load(65536 - 4)), trap(() => mem.load32(65536)), trap(() => mem.store(-8, 1)));
say(mem.size(), mem.grow(2), mem.size(), trap(() => mem.load(3 * 65536 - 8)), mem.load(3 * 65536 - 16), mem.grow(5), new Float64Array(mem.memory.buffer)[999].toFixed(6));

// 3. Imports, a table and indirect calls, unreachable, a NaN truncated.
const calls = [];
const ind = instance(module({
    types: [[[I32], [I32]], [[I32, I32], [I32]], [[F64], [I64]], [[], []]],
    imports: [["env", "log", 0]],
    funcs: [
        [0, [], [...get(0), ...i32(3), 0x6c, 0x10, 0x00]],                         // 1: log(x * 3)
        [0, [], [...get(0), ...get(0), 0x6c]],                                     // 2: square
        [1, [], [...get(1), ...get(0), 0x11, 0x00, 0x00]],                         // 3: call_indirect type 0 table[i](x)
        [2, [], [...get(0), 0xb0]],                                                // 4: i64.trunc_f64_s
        [3, [], [0x00]],                                                           // 5: unreachable
        [1, [], [...get(0), ...get(1), 0x6a]],                                     // 6: (i32, i32) add, wrong type in the table
    ],
    table: 4,
    elems: [1, 2, 6],
    exports: [["call", 3], ["trunc", 4], ["boom", 5]],
}), { env: { log: x => { calls.push(x); return x + 1; } } });
let indirect = 0; for (let i = 0; i < 30000; i++) indirect = (indirect + ind.call(i % 2, i & 1023)) | 0;
say(calls.length, calls.slice(0, 4).join(), indirect, ind.call(1, 12), trap(() => ind.call(2, 1)), trap(() => ind.call(3, 1)), trap(() => ind.call(9, 1)));
say(ind.trunc(-123.9), ind.trunc(2 ** 52), trap(() => ind.trunc(NaN)), trap(() => ind.trunc(1e19)), trap(() => ind.boom()));
say(WebAssembly.validate(module({ types: [[[], []]], funcs: [[0, [], []]], exports: [] })), WebAssembly.validate(new Uint8Array([0, 97, 115, 109, 2, 0, 0, 0])));

// 4. SIMD (v128): i32x4.add, f32x4.mul, i8x16.add, i16x8.mul of memory, a lane extracted.
const simdOp = (...op) => [0xfd, ...op];
const bin = op => [...get(0), ...get(1), ...simdOp(0x00, 0x04, 0x00), ...get(2), ...simdOp(0x00, 0x04, 0x00), ...simdOp(...op), ...simdOp(0x0b, 0x04, 0x00)];
const simd = instance(module({
    types: [[[I32, I32, I32], []], [[I32], [I32]]],
    funcs: [
        [0, [], [...get(0), ...bin([0xae, 0x01]).slice(get(0).length)]],          // i32x4.add
        [0, [], [...get(0), ...bin([0xe6, 0x01]).slice(get(0).length)]],          // f32x4.mul
        [0, [], [...get(0), ...bin([0x6e]).slice(get(0).length)]],                // i8x16.add
        [0, [], [...get(0), ...bin([0x95, 0x01]).slice(get(0).length)]],          // i16x8.mul
        [1, [], [...get(0), ...simdOp(0x00, 0x04, 0x00), ...simdOp(0x1b, 0x02)]], // i32x4.extract_lane 2
    ],
    memory: [1, 1],
    exports: [["add", 0], ["mul", 1], ["add8", 2], ["mul16", 3], ["lane", 4], ["memory", 0, 0x02]],
}));
const i32s = new Int32Array(simd.memory.buffer), f32s = new Float32Array(simd.memory.buffer);
for (let i = 0; i < 4; i++) { i32s[i] = i * 1000 + 7; i32s[4 + i] = 3 - i; f32s[8 + i] = 1.5 + i; f32s[12 + i] = 1.0001; }
for (let i = 0; i < 20000; i++) { simd.add(64, 64, 16); simd.mul(32, 32, 48); simd.add8(80, 80, 0); simd.mul16(96, 0, 112); simd.add(112, 112, 16); }
say([...i32s.slice(16, 20)].join(), [...f32s.slice(8, 12)].map(x => x.toFixed(3)).join(), [...new Uint8Array(simd.memory.buffer, 80, 8)].join(), [...new Int16Array(simd.memory.buffer, 96, 8)].join(), simd.lane(64), simd.lane(0));
