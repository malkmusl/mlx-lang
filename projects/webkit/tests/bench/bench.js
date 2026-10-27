// A small offline JavaScript benchmark of the port's JavaScriptCore:
// loops, objects, arrays, strings, regular expressions and JSON, each
// timed apart. Runs in jsc (`jsc bench.js`, printed) and in a page
// (bench.html, written into the document for webcore-dump to print).
// Each section checks its result, so a wrong interpreter fails loudly.
(function () {
    var now = typeof preciseTime === "function" ? function () { return preciseTime() * 1000; }
        : typeof performance === "object" ? function () { return performance.now(); } : Date.now;
    var results = [];
    function section(name, f, expected) {
        var start = now();
        var value = f();
        var time = now() - start;
        if (value !== expected)
            throw new Error(name + ": " + value + " instead of " + expected);
        results.push(name + " " + Math.round(time) + " ms");
        return time;
    }
    var total = 0;
    total += section("loops", function () {
        var sum = 0;
        for (var i = 0; i < 1000000; i++)
            sum = (sum + i * 7 ^ (i >> 3)) | 0;
        return sum;
    }, -780638528);
    total += section("objects", function () {
        function Point(x, y) { this.x = x; this.y = y; }
        Point.prototype.add = function (other) { return new Point(this.x + other.x, this.y + other.y); };
        var p = new Point(0, 0);
        var step = new Point(1, 2);
        for (var i = 0; i < 100000; i++)
            p = p.add(step);
        var o = {};
        for (var i = 0; i < 20000; i++)
            o["k" + (i % 500)] = i;
        return p.x + p.y + Object.keys(o).length;
    }, 300500);
    total += section("arrays", function () {
        var a = [];
        for (var i = 0; i < 50000; i++)
            a.push((i * 7919) % 10007);
        a.sort(function (x, y) { return x - y; });
        var squares = a.map(function (x) { return x * x; });
        return squares.reduce(function (s, x) { return (s + x) % 1000003; }, 0) + a[1000];
    }, 46281);
    total += section("strings", function () {
        var s = "";
        for (var i = 0; i < 50000; i++)
            s += String.fromCharCode(97 + i % 26);
        var count = 0;
        for (var i = 0; i < 2000; i++)
            count += s.indexOf("xyz", i * 20) >= 0 ? 1 : 0;
        var parts = s.split("a").join("-").toUpperCase();
        return count + parts.length;
    }, 52000);
    total += section("regexp", function () {
        var text = "";
        for (var i = 0; i < 2000; i++)
            text += "user" + i + "@example" + (i % 13) + ".com, ";
        var matches = text.match(/[a-z]+\d+@[a-z]+\d+\.com/g).length;
        var replaced = text.replace(/(\d+)/g, function (m) { return m.length; });
        var words = 0;
        var re = /\b(\w)(\w*)\b/g;
        while (re.exec(replaced))
            words++;
        return matches + replaced.length + words;
    }, 48000);
    total += section("json", function () {
        var data = [];
        for (var i = 0; i < 5000; i++)
            data.push({ id: i, name: "item" + i, tags: ["a", "b", i % 7], nested: { value: i / 4, flag: i % 2 == 0 } });
        var text = JSON.stringify(data);
        var back = JSON.parse(text);
        var sum = 0;
        for (var i = 0; i < back.length; i++)
            sum += back[i].nested.value + back[i].tags[2];
        return text.length + Math.round(sum);
    }, 3570211);
    results.push("total " + Math.round(total) + " ms");
    var report = results.join("\n");
    if (typeof document === "object" && document.body) {
        var pre = document.createElement("pre");
        pre.textContent = report;
        document.body.appendChild(pre);
    } else {
        print(report);
    }
})();
