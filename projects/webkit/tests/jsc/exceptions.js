if (typeof print === "undefined") globalThis.print = console.log;
// Exceptions caught where they are thrown from, in functions hot enough
// for the Baseline JIT and the DFG: thrown by a direct eval's code, by a
// function the eval calls, by a native function calling back into script
// (String.prototype.replace, JSON.parse's reviver), by a comparator the
// DFG inlines into Array.prototype.sort. (A JIT operation's exception was
// lost when mlx-cc chose the wrong conversion of its result: the throw
// escaped its try.)
function thrower(i) { throw new Error("x" + i); }
function viaEval(code) { try { eval(code); } catch (e) { return e.message; } return "none"; }
function viaCachedEval() { try { eval("throw new Error('cached')"); } catch (e) { return e.message; } return "none"; }
function viaReplace(i) { try { "a".replace("a", () => thrower(i)); } catch (e) { return e.message; } return "none"; }
function viaReviver(i) { try { JSON.parse("[1]", () => thrower(i)); } catch (e) { return e.message; } return "none"; }
function viaSort(i) { try { [i, 2, 1].sort(function (a, b) { thrower(i); return a - b; }); } catch (e) { return e.message; } return "none"; }
function sorted(i) { return [i % 7, 3, i % 5, 1].sort((a, b) => a - b).join(""); }
const counts = {};
function count(name, value, expected) { counts[name] = (counts[name] || 0) + (value === expected ? 1 : 0); }
for (let i = 0; i < 3000; i++) {
    try {
        count("eval", viaEval("thrower(" + i + ")"), "x" + i);
        count("cached", viaCachedEval(), "cached");
        count("replace", viaReplace(i), "x" + i);
        count("reviver", viaReviver(i), "x" + i);
        count("sort", viaSort(i), "x" + i);
        count("sorted", sorted(i), [i % 7, 3, i % 5, 1].sort((a, b) => a - b).join(""));
    } catch (e) {
        print("escaped at", i, e.message);
        break;
    }
}
print(Object.entries(counts).map(([k, v]) => k + " " + v).join(", "));
