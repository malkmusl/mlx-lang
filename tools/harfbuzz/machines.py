import re
# Converts HarfBuzz 8.3's Ragel-generated syllable machines (indic, khmer,
# myanmar, use) into tables for std/src/shaping/ragel.mlx: the state
# tables (as u16) and, per action number, a small program of operations.
import os
# (tools/harfbuzz/generate_tables.sh runs it: SRC is HarfBuzz 8.3's src
# directory, WORK where it put the preprocessed tables.)
SRC = os.environ['HB_SRC']
WORK = os.environ.get('HB_WORK', '.')

OPS = {'END': 0, 'TS_P': 1, 'TS_0': 2, 'TE_P1': 3, 'TE_P_DEC': 4, 'P_TE': 5, 'FOUND': 6, 'BROKEN': 7, 'ACT': 8, 'SWITCH': 9}
ARRAYS = ['trans_keys', 'key_spans', 'index_offsets', 'indicies', 'trans_targs', 'trans_actions', 'to_state_actions', 'from_state_actions', 'eof_trans']

def strip(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return re.sub(r'#line[^\n]*', '', s)

def statements(block, types):
    """The operations of an action's code."""
    ops = []
    i = 0
    text = block
    while i < len(text):
        rest = text[i:]
        m = re.match(r'\s+|[{};]', rest)
        if m and m.group(0):
            i += len(m.group(0)); continue
        m = re.match(r'te = p\+1', rest)
        if m: ops += [OPS['TE_P1']]; i += m.end(); continue
        m = re.match(r'te = p;p--', rest)
        if m: ops += [OPS['TE_P_DEC']]; i += m.end(); continue
        m = re.match(r'p = \(\(te\)\)-1', rest)
        if m: ops += [OPS['P_TE']]; i += m.end(); continue
        m = re.match(r'ts = p', rest)
        if m: ops += [OPS['TS_P']]; i += m.end(); continue
        m = re.match(r'ts = 0', rest)
        if m: ops += [OPS['TS_0']]; i += m.end(); continue
        m = re.match(r'found_syllable \((\w+)\)', rest)
        if m: ops += [OPS['FOUND'], types[m.group(1)]]; i += m.end(); continue
        m = re.match(r'buffer->scratch_flags \|= HB_BUFFER_SCRATCH_FLAG_HAS_BROKEN_SYLLABLE', rest)
        if m: ops += [OPS['BROKEN']]; i += m.end(); continue
        m = re.match(r'act = (\d+)', rest)
        if m: ops += [OPS['ACT'], int(m.group(1))]; i += m.end(); continue
        m = re.match(r'switch\( act \)', rest)
        if m:
            i += m.end()
            # the switch's body: balanced braces
            j = text.index('{', i)
            depth = 0
            for k in range(j, len(text)):
                if text[k] == '{': depth += 1
                elif text[k] == '}':
                    depth -= 1
                    if depth == 0: break
            inner = text[j + 1:k]
            cases = re.split(r'case (\d+):', inner)[1:]
            entries = []
            for n in range(0, len(cases), 2):
                sub = statements(cases[n + 1].replace('break', ''), types)
                entries.append((int(cases[n]), sub))
            ops += [OPS['SWITCH'], len(entries)]
            for value, sub in entries:
                ops += [value, len(sub)] + sub
            i = k + 1
            continue
        m = re.match(r'break', rest)
        if m: i += m.end(); continue
        raise Exception('unknown statement: ' + rest[:80])
    return ops

def actions(code, switch_on, types):
    """Per action number, its program, from the switch on `switch_on`."""
    a = code.index('switch ( %s' % switch_on)
    j = code.index('{', a)
    depth = 0
    for k in range(j, len(code)):
        if code[k] == '{': depth += 1
        elif code[k] == '}':
            depth -= 1
            if depth == 0: break
    inner = code[j + 1:k]
    # top-level "case N:" labels (brace depth 0)
    labels = []
    depth = 0
    i = 0
    while i < len(inner):
        ch = inner[i]
        if ch == '{': depth += 1
        elif ch == '}': depth -= 1
        elif depth == 0:
            m = re.match(r'case (\d+):', inner[i:])
            if m:
                labels.append((int(m.group(1)), i, i + m.end()))
                i += m.end()
                continue
        i += 1
    progs = {}
    for n, (value, a0, a1) in enumerate(labels):
        end = labels[n + 1][1] if n + 1 < len(labels) else len(inner)
        body = inner[a1:end]
        body = body[:body.rindex('break;')]
        progs[value] = statements(body, types)
    return progs

def convert(name):
    src = open(SRC + '/hb-ot-shaper-%s-machine.hh' % name).read()
    code = strip(src)
    enum = re.search(r'enum %s_syllable_type_t \{(.*?)\};' % name, code, re.S).group(1)
    types = {}
    for n, t in enumerate(re.findall(r'(\w+),', enum)):
        types[t] = n
    tables = {}
    for arr in ARRAYS:
        m = re.search(r'static const [\w ]+ _%s_syllable_machine_%s\[\] = \{(.*?)\};' % (name, arr), code, re.S)
        tables[arr] = [int(x) for x in re.findall(r'-?\d+', m.group(1))]
    start = int(re.search(r'%s_syllable_machine_start = (\d+)' % name, code).group(1))
    exec_code = code[code.index('_resume:'):]
    progs = {}
    progs['from'] = actions(exec_code, '_%s_syllable_machine_from_state_actions' % name, types)
    progs['trans'] = actions(exec_code, '_%s_syllable_machine_trans_actions' % name, types)
    progs['to'] = actions(exec_code, '_%s_syllable_machine_to_state_actions' % name, types)
    # One program list: index = action number (from, trans and to share
    # numbers, as Ragel numbers actions once).
    allprogs = {}
    for kind in ('from', 'trans', 'to'):
        for k, v in progs[kind].items():
            if k in allprogs and allprogs[k] != v: raise Exception('conflict %s %d' % (name, k))
            allprogs[k] = v
    nmax = max(allprogs) + 1
    offsets = []
    program = [0]
    for k in range(nmax):
        if k in allprogs:
            offsets.append(len(program))
            program += allprogs[k] + [0]
        else:
            offsets.append(0)
    return tables, start, offsets, program, types

def arr(name, data, per=24):
    lines = []
    for i in range(0, len(data), per):
        lines.append('    ' + ', '.join(str(v) for v in data[i:i + per]) + ',')
    return 'pub var %s = [%d]u16{\n%s\n}\n' % (name, len(data), '\n'.join(lines))

out = ['''// The syllable machines of the Indic, Khmer, Myanmar and Universal shapers
// (made of HarfBuzz 8.3's Ragel-generated hb-ot-shaper-*-machine.hh):
// their state tables, and per action a program of ragel.mlx's operations.
const ragel = @import("./ragel.mlx")
''']
for name in ('indic', 'khmer', 'myanmar', 'use'):
    tables, start, offsets, program, types = convert(name)
    up = name.upper()
    out.append('// %s\npub const %s_START: u32 = %d\n' % (name, up, start))
    for a in ARRAYS:
        out.append(arr('%s_%s' % (up, a.upper()), [v & 0xFFFF for v in tables[a]]))
    out.append(arr('%s_PROGRAM_OFFSETS' % up, offsets))
    out.append(arr('%s_PROGRAM' % up, program))
    fields = [('transKeys', 'TRANS_KEYS'), ('keySpans', 'KEY_SPANS'), ('indexOffsets', 'INDEX_OFFSETS'), ('indicies', 'INDICIES'), ('transTargs', 'TRANS_TARGS'), ('transActions', 'TRANS_ACTIONS'), ('toStateActions', 'TO_STATE_ACTIONS'), ('fromStateActions', 'FROM_STATE_ACTIONS'), ('eofTrans', 'EOF_TRANS'), ('programOffsets', 'PROGRAM_OFFSETS'), ('program', 'PROGRAM')]
    body = '    mach.*.start = %s_START\n' % up + ''.join('    mach.*.%s = @intFromPtr(&%s_%s)\n' % (f, up, t) for f, t in fields)
    out.append('pub fn %s(mach: *ragel.Machine) -> void {\n%s}\n' % (name + 'Machine', body))
    print(name, start, {a: len(tables[a]) for a in ARRAYS}, len(program), types)
open('std/src/shaping/machines.mlx', 'w').write('\n'.join(out))
