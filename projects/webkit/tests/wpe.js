// JavaScript run by the WPE port's jsc (tools/check_wpe_jsc.sh).
function fibonacci(n) { return n < 2 ? n : fibonacci(n - 1) + fibonacci(n - 2); }
print("fibonacci(20) = " + fibonacci(20));
print([5, 3, 9, 1].sort((a, b) => a - b).join(","));
print(JSON.stringify({ name: "WPE", list: [1, 2, { deep: true }] }));
print("Straße".toUpperCase(), "İ".toLowerCase().length, "naïve".normalize("NFD").length);
print(new Intl.NumberFormat("de-DE").format(1234567.891));
print(/(\d+)-(\d+)/.exec("range 10-20").slice(1).join(" to "));
class Point {
    constructor(x, y) { this.x = x; this.y = y; }
    get length() { return Math.hypot(this.x, this.y); }
}
print(new Point(3, 4).length);
const map = new Map([["a", 1], ["b", 2]]);
print([...map.keys()].join("") + " " + [...map.values()].reduce((a, b) => a + b));
const proto = { greet() { return "hi " + this.name; } };
const withProto = Object.setPrototypeOf({ name: "proto" }, proto);
print(withProto.greet(), Object.getPrototypeOf(withProto) === proto);
let caught = "";
try { undefinedFunction(); } catch (e) { caught = e.constructor.name + ": " + e.message; }
print(caught);
print(typeof BigInt, (2n ** 64n).toString());
let total = 0;
for (let i = 0; i < 200000; i++) total += i & 15;
print("total " + total);
