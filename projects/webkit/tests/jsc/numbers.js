var out = print;
function t(label, f) { try { out(label + " => " + f()) } catch (e) { out(label + " => ERR " + e) } }
function parts(p) { return p.map(x => x.type + ":" + x.value + (x.unit ? "(" + x.unit + ")" : "")).join("|") }
var style = arguments[0] || "currency";
for (const c of ["EUR", "USD", "JPY", "GBP", "CHF", "CAD", "AUD", "CNY", "INR", "BRL"]) {
  t(c, () => [1, 2, 5, 1.5].map(v => new Intl.NumberFormat("en", style === "currency" ? {style: "currency", currency: c} : {style: "percent"}).format(v)).join(" / "));
}
t("parts", () => parts(new Intl.NumberFormat("en", {style: "currency", currency: "EUR"}).formatToParts(1234.5)));
t("parts", () => parts(new Intl.NumberFormat("en").formatToParts(1234.5)));
