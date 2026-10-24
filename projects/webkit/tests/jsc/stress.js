if (typeof print === "undefined") globalThis.print = console.log;
// 1. Objects, classes, inheritance, getters
class Shape { constructor(n) { this.name = n } area() { return 0 } toString() { return `${this.name}:${this.area().toFixed(3)}` } }
class Circle extends Shape { constructor(r) { super("circle"); this.r = r } area() { return Math.PI * this.r ** 2 } }
class Rect extends Shape { constructor(w, h) { super("rect"); this.w = w; this.h = h } area() { return this.w * this.h } get diag() { return Math.hypot(this.w, this.h) } }
const shapes = []; for (let i = 1; i <= 2000; i++) shapes.push(i % 3 ? new Circle(i / 7) : new Rect(i, i / 3));
shapes.sort((a, b) => a.area() - b.area());
print(shapes.slice(0, 3).join(" "), shapes.at(-1).toString(), shapes.filter(s => s instanceof Rect).reduce((a, s) => a + s.diag, 0).toFixed(4));
// 2. Closures & recursion
const memo = new Map(); const fib = n => n < 2 ? n : memo.get(n) ?? (memo.set(n, fib(n - 1) + fib(n - 2)), memo.get(n));
print(fib(90), [...Array(20).keys()].map(fib).join(","));
function ack(m, n) { return m === 0 ? n + 1 : n === 0 ? ack(m - 1, 1) : ack(m - 1, ack(m, n - 1)) } print(ack(2, 300));
// 3. Strings & regex
let text = ""; for (let i = 0; i < 5000; i++) text += `item-${i}=${(i * 7919) % 10007};`;
const re = /item-(\d+)=(\d+);/g; let m, sum = 0, cnt = 0; while ((m = re.exec(text))) { sum += +m[2]; cnt++ }
print(cnt, sum, text.length, text.replace(/\d+/g, d => d.length).slice(0, 40));
print("The Quick Brown Fox".split(/\s+/).map(w => w[0].toLowerCase() + w.slice(1).toUpperCase()).join("_"), "a,b,,c".split(","), "abc".localeCompare("abd"));
print(/(?<year>\d{4})-(?<m>\d\d)/u.exec("on 2024-03-15").groups.year, "x".padEnd(5, "ab"), "ﬃ".toUpperCase(), "ǅ".toLowerCase(), /\p{L}+/u.exec("123äöü")[0]);
// 4. JSON
const big = { list: Array.from({ length: 300 }, (_, i) => ({ id: i, name: "n" + i, tags: ["a", "b"].slice(i % 2), val: i / 3, nested: { ok: i % 5 === 0, nul: null } })) };
const js = JSON.stringify(big); const back = JSON.parse(js); print(js.length, back.list[299].val, JSON.stringify(back.list[7]), JSON.stringify({ d: new Date(0), u: undefined, f() {} , n: NaN }, null, 1).length);
// 5. Typed arrays and math
const f64 = new Float64Array(10000); for (let i = 0; i < f64.length; i++) f64[i] = Math.sin(i) * Math.exp(-i / 5000);
let acc = 0; for (const v of f64) acc += v * v; print(acc.toFixed(10), Math.max(...f64.subarray(0, 100)).toFixed(12), new Int32Array(f64.buffer, 0, 4).join());
const u8 = new Uint8Array(256).map((_, i) => (i * 31) & 255); print(u8.reduce((a, b) => (a * 33 + b) >>> 0, 5381), u8.slice(250).join(), [...new Uint16Array(new Uint8Array([1, 2, 3, 4]).buffer)].join());
print((0.1).toFixed(20), (1e21).toFixed(2), (123.456).toPrecision(10), (-1.5).toString(2), (255).toString(36), parseFloat("1e-7"), 2 ** 53 + 1, Number("0x1F"), Math.round(-0.5), Math.fround(1.1));
// 6. Map/Set/WeakMap, symbols, iterators
const ms = new Map(); for (let i = 0; i < 10000; i++) ms.set("k" + (i % 997), i); const st = new Set(text.match(/\d/g)); print(ms.size, ms.get("k5"), st.size, [...st].sort().join(""));
const it = { *[Symbol.iterator]() { for (let i = 0; i < 5; i++) yield i * i } }; print([...it].join(), Array.from(it, x => x + 1).join(), Object.fromEntries([["a", 1], ["b", 2]]).b);
// 7. Proxy/Reflect, getters, defineProperty
const log = []; const p = new Proxy({ a: 1 }, { get(t, k, r) { log.push(String(k)); return Reflect.get(t, k, r) }, has(t, k) { return k === "magic" || k in t } });
print(p.a, "magic" in p, log.join()); const o = {}; Object.defineProperty(o, "x", { get() { return 42 }, enumerable: false }); print(o.x, Object.keys(o).length, JSON.stringify(Object.getOwnPropertyDescriptor(o, "x")));
// 8. Promises & async
const out = []; (async () => { for (let i = 0; i < 3; i++) out.push(await new Promise(r => r(i * 2))); out.push("done") })();
Promise.all([1, Promise.resolve(2), 3]).then(v => out.push(v.join("+"))); Promise.allSettled([Promise.reject(new Error("x"))]).then(r => out.push(r[0].status));
if (typeof drainMicrotasks === "function") drainMicrotasks(); setTimeout(() => print(out.join(" ")), 0);
// 9. Labels, destructuring, spread, optional chaining, BigInt
const { a = 5, b: { c } = { c: 9 }, ...rest } = { x: 1, y: 2 }; print(a, c, JSON.stringify(rest), null?.foo, ({ f: { g: () => 3 } }).f?.g?.());
print(2n ** 200n % 1000000007n, BigInt.asUintN(8, 257n), (123456789012345678901234567890n * 98765432109876543210n).toString(36), BigInt(-17) / 5n, -17n % 5n);
// 10. Date
const d = new Date(Date.UTC(2024, 1, 29, 23, 59, 59, 999)); print(d.toISOString(), d.getUTCDay(), new Date(d.getTime() + 1).toISOString(), Date.parse("2024-02-29T23:59:59.999Z") === d.getTime(), new Date(8.64e15 + 1).getTime());
print(new Date(2024, 0, 31).setMonth(1), new Date("March 7, 2021 10:00:00 UTC").toUTCString(), new Date(0).toLocaleString("en-US", { timeZone: "UTC" }));
