#!/usr/bin/env python3
"""Writes projects/cc/names.mlx: the names mlx-cc interns first, in a fixed
order, so each has a constant id (punctuators P_*, keywords K_*, the
preprocessor's words D_*).

    python3 tools/cc/gen_names.py > projects/cc/names.mlx
"""
puncts = [
    ("ELLIPSIS", "..."), ("SHL_ASSIGN", "<<="), ("SHR_ASSIGN", ">>="),
    ("EQ", "=="), ("NE", "!="), ("LE", "<="), ("GE", ">="), ("ARROW", "->"),
    ("INC", "++"), ("DEC", "--"), ("SHL", "<<"), ("SHR", ">>"), ("ANDAND", "&&"),
    ("OROR", "||"), ("ADD_ASSIGN", "+="), ("SUB_ASSIGN", "-="), ("MUL_ASSIGN", "*="),
    ("DIV_ASSIGN", "/="), ("MOD_ASSIGN", "%="), ("AND_ASSIGN", "&="), ("OR_ASSIGN", "|="),
    ("XOR_ASSIGN", "^="), ("HASHHASH", "##"),
    ("LPAREN", "("), ("RPAREN", ")"), ("LBRACE", "{"), ("RBRACE", "}"), ("LBRACKET", "["),
    ("RBRACKET", "]"), ("SEMI", ";"), ("COMMA", ","), ("DOT", "."), ("PLUS", "+"),
    ("MINUS", "-"), ("STAR", "*"), ("SLASH", "/"), ("PERCENT", "%"), ("AMP", "&"),
    ("PIPE", "|"), ("CARET", "^"), ("TILDE", "~"), ("BANG", "!"), ("QUESTION", "?"),
    ("COLON", ":"), ("ASSIGN", "="), ("LT", "<"), ("GT", ">"), ("HASH", "#"),
]
keywords = [
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
    "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long",
    "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct",
    "switch", "typedef", "union", "unsigned", "void", "volatile", "while",
    "_Bool", "_Alignas", "_Alignof", "_Atomic", "_Generic", "_Noreturn",
    "_Static_assert", "_Thread_local", "__attribute__", "__restrict", "__restrict__",
    "__inline", "__inline__", "__const", "__volatile__", "__extension__", "__asm__",
    "asm", "__typeof__", "typeof", "__signed__", "__builtin_va_list",
    "__builtin_va_start", "__builtin_va_end", "__builtin_va_arg", "__builtin_va_copy",
    "__builtin_offsetof", "__alignof__", "__thread",
]
directives = [
    "include", "include_next", "define", "undef", "if", "ifdef", "ifndef", "elif",
    "else", "endif", "error", "warning", "pragma", "line", "defined", "__VA_ARGS__",
    "once", "__FILE__", "__LINE__", "__COUNTER__", "__DATE__", "__TIME__",
    "__has_include", "__has_include_next", "__BASE_FILE__", "__VA_OPT__", "main",
    "_start", "exit", "__mlxcc_init",
]

# A single leading underscore (C11's _Bool) is dropped; GNU's spellings
# with two keep a GNU_ (or GNU2_, with two trailing ones too) in front.
OVERRIDES = {
    "__VA_ARGS__": "VA_ARGS", "__FILE__": "FILE_MACRO", "__LINE__": "LINE_MACRO",
    "__COUNTER__": "COUNTER", "__DATE__": "DATE", "__TIME__": "TIME",
    "__has_include": "HAS_INCLUDE", "__has_include_next": "HAS_INCLUDE_NEXT",
    "__BASE_FILE__": "BASE_FILE", "__VA_OPT__": "VA_OPT",
    "__mlxcc_init": "MLXCC_INIT", "_start": "START",
}

def const_name(word):
    if word in OVERRIDES:
        return OVERRIDES[word]
    lead = len(word) - len(word.lstrip("_"))
    trail = len(word) - len(word.rstrip("_"))
    core = word.strip("_").upper()
    if lead == 2 and trail == 2:
        return "GNU2_" + core
    if lead == 2:
        return "GNU_" + core
    return core

entries = []
for name, text in puncts:
    entries.append(("P_" + name, text))
for word in keywords:
    entries.append(("K_" + const_name(word), word))
seen = set(text for _, text in entries)
for word in directives:
    if word in seen:
        continue
    entries.append(("D_" + const_name(word), word))

names = [n for n, _ in entries]
assert len(names) == len(set(names)), "duplicate constant"

out = []
out.append("// The names mlx-cc interns first, in this order, so each has a constant")
out.append("// id: the punctuators (P_*), the keywords (K_*) and the preprocessor's")
out.append("// words (D_*; \"if\", \"else\" and the like are the keywords' ids).")
out.append("// Written by tools/cc/gen_names.py; do not edit by hand.")
out.append('const base = @import("./base.mlx")')
out.append("")
for index, (name, text) in enumerate(entries, start=1):
    out.append(f"pub const {name}: u32 = {index}")
out.append("")
out.append(f"pub const FIRST_PUNCT: u32 = 1")
out.append(f"pub const LAST_PUNCT: u32 = {len(puncts)}")
out.append(f"pub const FIRST_KEYWORD: u32 = {len(puncts) + 1}")
out.append(f"pub const LAST_KEYWORD: u32 = {len(puncts) + len(keywords)}")
out.append("")
out.append("fn fixed(text: []const u8, id: u32) -> void {")
out.append("    if base.intern(text) != id { base.fatal(\"the fixed names are out of order\") }")
out.append("}")
out.append("")
out.append("// Interns the fixed names; first thing after base.init.")
out.append("pub fn init() -> void {")
for name, text in entries:
    out.append(f'    fixed("{text}", {name})')
out.append("}")
print("\n".join(out))
