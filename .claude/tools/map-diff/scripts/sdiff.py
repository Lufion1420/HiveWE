"""Per-function diff of two Warcraft III map scripts (war3map.lua or war3map.j).

usage: sdiff.py <orig> <new> [--json OUT] [--top N] [--max-diff LINES] [--float-digits D]

Splits each script into the globals and the top-level functions, normalizes them
(whitespace, comments, `;`, number formatting rounded to D decimals, string quote
style) and reports:
  globals   missing (in orig only), extra (in new only), changed (value differs)
  functions missing, extra, reordered (same lines, other order), changed (with a
            short unified diff of normalized lines)
Lua is parsed with a small tokenizer that tracks block depth, so user code with
nested or anonymous functions works. JASS is parsed line by line (globals/endglobals,
function/endfunction). Pure stdlib; run with `python -I`.
Exit code: 0 identical after normalization, 1 differences, 2 usage/parse error.
"""
import sys, re, json, difflib, argparse
from collections import Counter

# ---------------------------------------------------------------- numbers / strings

FLOAT_DIGITS = 3
LOOSE = True      # --strict turns off paren / `= nil` normalization

def norm_number(tok):
    try:
        if tok.lower().startswith('0x'):
            v = int(tok, 16)
        elif tok.startswith('$'):            # JASS hex
            v = int(tok[1:], 16)
        elif re.fullmatch(r'\d+', tok) and not (tok.startswith('0') and len(tok) > 1):
            return tok
        elif re.fullmatch(r'0[0-7]+', tok):   # JASS octal
            v = int(tok, 8)
        else:
            v = float(tok)
    except ValueError:
        return tok
    if isinstance(v, int):
        return str(v)
    s = ('%.*f' % (FLOAT_DIGITS, round(v, FLOAT_DIGITS)))
    s = s.rstrip('0').rstrip('.') if '.' in s else s
    return '0' if s in ('-0', '') else s

# ---------------------------------------------------------------- Lua

LUA_NAME = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
LUA_NUM = re.compile(r'0[xX][0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?(?:[pP][+-]?\d+)?|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?')
LUA_SYM = ['...', '..', '==', '~=', '<=', '>=', '//', '::', '<<', '>>']

def lua_tokens(src):
    """Yield (kind, text, line). Comments are dropped."""
    i, n, line = 0, len(src), 1
    out = []
    while i < n:
        c = src[i]
        if c == '\n':
            line += 1; i += 1; continue
        if c in ' \t\r\f\v':
            i += 1; continue
        if src.startswith('--', i):
            m = re.match(r'--\[(=*)\[', src[i:])
            if m:
                close = ']' + m.group(1) + ']'
                j = src.find(close, i + len(m.group(0)))
                j = n if j < 0 else j + len(close)
            else:
                j = src.find('\n', i); j = n if j < 0 else j
            line += src.count('\n', i, j); i = j; continue
        m = re.match(r'\[(=*)\[', src[i:i + 64]) if c == '[' else None
        if m:
            close = ']' + m.group(1) + ']'
            j = src.find(close, i + len(m.group(0)))
            j = n if j < 0 else j + len(close)
            body = src[i + len(m.group(0)):j - len(close)]
            out.append(('str', '"' + body.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"', line))
            line += src.count('\n', i, j); i = j; continue
        if c in '"\'':
            j = i + 1; buf = []
            while j < n and src[j] != c:
                if src[j] == '\\' and j + 1 < n:
                    nxt = src[j + 1]
                    buf.append("'" if nxt == "'" else '\\' + nxt)   # canonical: double-quoted
                    j += 2; continue
                if src[j] == '\n': break
                buf.append('\\"' if src[j] == '"' else src[j]); j += 1
            out.append(('str', '"' + ''.join(buf) + '"', line)); i = j + 1; continue
        m = LUA_NUM.match(src, i) if (c.isdigit() or (c == '.' and i + 1 < n and src[i + 1].isdigit())) else None
        if m:
            out.append(('num', norm_number(m.group(0)), line)); i = m.end(); continue
        m = LUA_NAME.match(src, i)
        if m:
            out.append(('name', m.group(0), line)); i = m.end(); continue
        for s in LUA_SYM:
            if src.startswith(s, i):
                out.append(('sym', s, line)); i += len(s); break
        else:
            if c != ';':
                out.append(('sym', c, line))
            i += 1
    return out

OPEN = {'function', 'if', 'do', 'repeat'}
CLOSE = {'end', 'until'}

def wrapped(seq):
    """True if seq is '(' ... ')' with the first paren closing at the end."""
    if len(seq) < 2 or seq[0] != '(' or seq[-1] != ')': return False
    d = 0
    for idx, t in enumerate(seq):
        if t == '(': d += 1
        elif t == ')':
            d -= 1
            if d == 0: return idx == len(seq) - 1
    return False

def tidy(line):
    """Drop parens that wrap a whole condition / return value / assigned value,
    and `local x = nil` -> `local x`. World Editor Lua wraps these, HiveWE may not."""
    if LOOSE:
        if line and line[0] in ('if', 'elseif', 'while') and line[-1] in ('then', 'do'):
            inner = line[1:-1]
            while wrapped(inner): inner = inner[1:-1]
            line = [line[0]] + inner + [line[-1]]
        elif line and line[0] == 'return':
            inner = line[1:]
            while wrapped(inner): inner = inner[1:-1]
            line = ['return'] + inner
        else:
            d = 0
            for idx, t in enumerate(line):
                if t in '([{': d += 1
                elif t in ')]}': d -= 1
                elif t == '=' and d == 0:
                    inner = line[idx + 1:]
                    while wrapped(inner): inner = inner[1:-1]
                    line = line[:idx + 1] + inner
                    break
    if len(line) == 4 and line[0] == 'local' and line[2:] == ['=', 'nil']:
        line = line[:2]
    return line

WE_LOOP_BREAK = re.compile(r'^if [( ]*(\w+) > (\S+?)[ )]* then break end$')

def fold_we_loops(lines):
    """World Editor array init: `i = 0 / while true do / if ((i > N)) then break end /
    body / i = i + 1 / end`  ->  `for i = 0 , N do / body / end` (HiveWE's form)."""
    out, k = [], 0
    while k < len(lines):
        m = WE_LOOP_BREAK.match(lines[k + 2]) if k + 2 < len(lines) else None
        if m and lines[k + 1] == 'while true do' and lines[k] == '%s = 0' % m.group(1):
            v = m.group(1); j = k + 3; body = []
            while j + 1 < len(lines) and not (lines[j] == '%s = %s + 1' % (v, v) and lines[j + 1] == 'end'):
                body.append(lines[j]); j += 1
            if j + 1 < len(lines):
                out += ['for %s = 0 , %s do' % (v, m.group(2))] + body + ['end']; k = j + 2; continue
        out.append(lines[k]); k += 1
    return out

def lines_of(toks):
    """Group tokens by source line into normalized text lines."""
    res, cur, cl = [], [], None
    for k, t, l in toks:
        if cl is not None and l != cl:
            res.append(' '.join(tidy(cur))); cur = []
        cur.append(t); cl = l
    if cur: res.append(' '.join(tidy(cur)))
    return fold_we_loops(res) if LOOSE else res

def parse_lua(src):
    toks = lua_tokens(src)
    funcs, globs, order = {}, {}, []
    i, n = 0, len(toks)

    def skip_block(j):
        """toks[j] is an opener; return index after its closer."""
        depth = 0
        while j < n:
            k, t, _ = toks[j]
            if k == 'name':
                if t in OPEN: depth += 1
                elif t in CLOSE:
                    depth -= 1
                    if depth == 0:
                        if t == 'until':     # until <expr>: take rest of line
                            ln = toks[j][2]; j += 1
                            while j < n and toks[j][2] == ln: j += 1
                            return j
                        return j + 1
            j += 1
        return n

    CONT = ('..', 'and', 'or', '+', '-', '*', '/', '%', '^', '==', '~=', '<', '>', '<=', '>=')

    def stmt_end(e):
        """Index after the statement whose tokens continue from e."""
        depth = bdepth = 0
        while e < n:
            kk, tt, ll = toks[e]
            if tt in '([{': depth += 1
            elif tt in ')]}': depth -= 1
            elif kk == 'name' and tt in OPEN: bdepth += 1
            elif kk == 'name' and tt in CLOSE: bdepth -= 1
            e += 1
            if depth <= 0 and bdepth <= 0 and (e >= n or toks[e][2] != ll):
                if not (e < n and (toks[e][1] in CONT or tt in CONT or tt in (',', '='))):
                    break
        return e

    def add(store, name, val):
        key, c = name, 2
        while key in store:
            key = '%s#%d' % (name, c); c += 1
        store[key] = val; return key

    while i < n:
        k, t, l = toks[i]
        start = i
        is_local = (k == 'name' and t == 'local' and i + 1 < n and toks[i + 1][1] == 'function')
        if (k == 'name' and t == 'function') or is_local:
            fi = i + 1 if is_local else i
            j = fi + 1; parts = []
            while j < n and (toks[j][0] == 'name' or toks[j][1] in '.:'):
                parts.append(toks[j][1]); j += 1
            name = ''.join(parts) or '<anon>'
            end = skip_block(fi)
            key = add(funcs, name, lines_of(toks[start:end])); order.append(('f', key))
            i = end; continue
        lstart = i + 1 if (k == 'name' and t == 'local') else i
        if lstart < n and toks[lstart][0] == 'name' and toks[lstart][1] not in OPEN | CLOSE | {'local', 'return', 'goto', 'break'}:
            # assignment: [local] name[.x][...] [, name] = expr   (else a call statement)
            j = lstart; depth = 0
            while j < n and j - lstart <= 64:
                tt = toks[j][1]
                if tt in '([{': depth += 1
                elif tt in ')]}': depth -= 1
                elif depth == 0 and tt == '=': break
                elif depth == 0 and j > lstart and toks[j][2] != toks[j - 1][2] and toks[j - 1][1] not in ',(=':
                    break
                j += 1
            prefix = 'local ' if lstart > i else ''
            if j < n and toks[j][1] == '=':
                lhs = prefix + ''.join(x[1] for x in toks[lstart:j])
                e = stmt_end(j + 1)
                key = add(globs, lhs, ' '.join(x[1] for x in toks[j + 1:e])); order.append(('g', key))
                i = e; continue
            if prefix:                          # "local x" without a value
                e = stmt_end(lstart)
                key = add(globs, prefix + ''.join(x[1] for x in toks[lstart:e]), 'nil'); order.append(('g', key))
                i = e; continue
        # other top-level statement (call, do-block, if, ...): matched by content later
        end = skip_block(i) if (k == 'name' and t in OPEN) else stmt_end(i)
        key = add(funcs, '<toplevel>', lines_of(toks[start:end]))
        order.append(('t', key)); i = end
    return globs, funcs

# ---------------------------------------------------------------- JASS

def jass_strip(line):
    out, i, ins, q = [], 0, False, ''
    while i < len(line):
        c = line[i]
        if ins:
            out.append(c)
            if c == '\\' and i + 1 < len(line): out.append(line[i + 1]); i += 2; continue
            if c == q: ins = False
        elif c in '"\'':
            ins, q = True, c; out.append(c)
        elif line.startswith('//', i):
            break
        else:
            out.append(c)
        i += 1
    return ''.join(out)

JTOK = re.compile(r'"(?:\\.|[^"\\])*"|\'[^\']*\'|\$[0-9a-fA-F]+|0[xX][0-9a-fA-F]+|\d+\.?\d*|\.\d+|[A-Za-z_]\w*|==|!=|<=|>=|\S')

def jass_norm(line):
    return ' '.join(norm_number(t) if re.match(r'[\d.$]', t) and t != '.' else t for t in JTOK.findall(jass_strip(line)))

def parse_jass(src):
    globs, funcs = {}, {}
    lines = src.splitlines(); i = 0; top = 0
    while i < len(lines):
        s = jass_norm(lines[i])
        if s == 'globals':
            i += 1
            while i < len(lines) and jass_norm(lines[i]) != 'endglobals':
                g = jass_norm(lines[i])
                if g:
                    m = re.match(r'(?:constant )?(\w+) (?:array )?(\w+)(?: = (.*))?$', g)
                    name = m.group(2) if m else g
                    globs[name] = g
                i += 1
        elif s.startswith(('function ', 'constant function ')):
            name = s.split()[2 if s.startswith('constant') else 1]
            body = [s]; i += 1
            while i < len(lines):
                b = jass_norm(lines[i])
                if b: body.append(b)
                if b == 'endfunction': break
                i += 1
            key = name; c = 2
            while key in funcs: key = '%s#%d' % (name, c); c += 1
            funcs[key] = body
        elif s and not s.startswith('native '):
            funcs['<toplevel@%d>' % top] = [s]; top += 1
        i += 1
    return globs, funcs

# ---------------------------------------------------------------- compare

def is_jass(path, src):
    return path.lower().endswith('.j') or re.search(r'^\s*endfunction\b', src, re.M) is not None

def compare(orig_path, new_path, max_diff=40):
    a = open(orig_path, encoding='utf-8', errors='replace').read()
    b = open(new_path, encoding='utf-8', errors='replace').read()
    lang = 'jass' if is_jass(orig_path, a) else 'lua'
    parse = parse_jass if lang == 'jass' else parse_lua
    ga, fa = parse(a); gb, fb = parse(b)
    # top-level statements are matched by content, not position
    ta = Counter(l for k, v in fa.items() if k.startswith('<toplevel') for l in v)
    tb = Counter(l for k, v in fb.items() if k.startswith('<toplevel') for l in v)
    fa = {k: v for k, v in fa.items() if not k.startswith('<toplevel')}
    fb = {k: v for k, v in fb.items() if not k.startswith('<toplevel')}
    res = {'lang': lang, 'orig': orig_path, 'new': new_path,
           'counts': {'orig_functions': len(fa), 'new_functions': len(fb),
                      'orig_globals': len(ga), 'new_globals': len(gb)},
           'globals': {'missing': sorted(set(ga) - set(gb)), 'extra': sorted(set(gb) - set(ga)),
                       'changed': [{'name': k, 'orig': ga[k], 'new': gb[k]}
                                   for k in sorted(set(ga) & set(gb)) if ga[k] != gb[k]]},
           'toplevel': {'missing': sorted((ta - tb).elements()), 'extra': sorted((tb - ta).elements())},
           'functions': {'missing': sorted(set(fa) - set(fb)), 'extra': sorted(set(fb) - set(fa)),
                         'reordered': [], 'changed': []}}
    for k in sorted(set(fa) & set(fb)):
        x, y = fa[k], fb[k]
        if x == y: continue
        if sorted(x) == sorted(y):
            res['functions']['reordered'].append({'name': k, 'lines': len(x)}); continue
        cx, cy = Counter(x), Counter(y)
        diff = list(difflib.unified_diff(x, y, 'orig', 'new', n=1, lineterm=''))
        res['functions']['changed'].append({
            'name': k, 'orig_lines': len(x), 'new_lines': len(y),
            'removed': sum((cx - cy).values()), 'added': sum((cy - cx).values()),
            'diff': diff[2:2 + max_diff] + (['... (%d more diff lines)' % (len(diff) - 2 - max_diff)] if len(diff) - 2 > max_diff else [])})
    res['functions']['changed'].sort(key=lambda c: -(c['removed'] + c['added']))
    f, g = res['functions'], res['globals']
    res['identical'] = not (f['missing'] or f['extra'] or f['changed'] or f['reordered'] or
                            g['missing'] or g['extra'] or g['changed'] or res['toplevel']['missing'] or res['toplevel']['extra'])
    return res

def summary(res, top=15):
    f, g, t = res['functions'], res['globals'], res['toplevel']
    c = res['counts']
    def short(xs, k=12):
        return ', '.join(xs[:k]) + (' ... (+%d)' % (len(xs) - k) if len(xs) > k else '')
    o = ['%s: %d/%d functions, %d/%d globals (orig/new)%s' % (
        res['lang'], c['orig_functions'], c['new_functions'], c['orig_globals'], c['new_globals'],
        ' -- IDENTICAL after normalization' if res['identical'] else '')]
    o.append('globals: %d missing, %d extra, %d changed' % (len(g['missing']), len(g['extra']), len(g['changed'])))
    if g['missing']: o.append('  missing: ' + short(g['missing']))
    if g['extra']: o.append('  extra: ' + short(g['extra']))
    for ch in g['changed'][:5]: o.append('  changed %s: %s -> %s' % (ch['name'], ch['orig'][:60], ch['new'][:60]))
    o.append('functions: %d missing, %d extra, %d changed, %d reordered-only' % (
        len(f['missing']), len(f['extra']), len(f['changed']), len(f['reordered'])))
    if f['missing']: o.append('  missing: ' + short(f['missing']))
    if f['extra']: o.append('  extra: ' + short(f['extra']))
    if f['reordered']: o.append('  reordered: ' + short([r['name'] for r in f['reordered']]))
    for ch in f['changed'][:top]:
        o.append('  ~ %-40s -%d +%d  (%d -> %d lines)' % (ch['name'], ch['removed'], ch['added'], ch['orig_lines'], ch['new_lines']))
        ex = [d for d in ch['diff'] if d[:1] in '+-'][:4]
        for d in ex: o.append('      ' + d[:150])
    if len(f['changed']) > top: o.append('  ... %d more changed functions (see JSON)' % (len(f['changed']) - top))
    if t['missing'] or t['extra']:
        o.append('top-level statements: %d missing, %d extra' % (len(t['missing']), len(t['extra'])))
    return '\n'.join(o)

def main():
    global FLOAT_DIGITS, LOOSE
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('orig'); ap.add_argument('new')
    ap.add_argument('--json', help='write full JSON result here')
    ap.add_argument('--top', type=int, default=15, help='changed functions shown in the text summary')
    ap.add_argument('--max-diff', type=int, default=40, help='unified diff lines kept per function')
    ap.add_argument('--float-digits', type=int, default=3)
    ap.add_argument('--strict', action='store_true', help='keep redundant parens, `local x = nil` and World Editor while/break loops')
    a = ap.parse_args()
    FLOAT_DIGITS = a.float_digits; LOOSE = not a.strict
    try:
        res = compare(a.orig, a.new, a.max_diff)
    except (OSError, UnicodeError) as e:
        print('error:', e, file=sys.stderr); sys.exit(2)
    if a.json:
        json.dump(res, open(a.json, 'w', encoding='utf-8'), indent=1)
    print(summary(res, a.top))
    sys.exit(0 if res['identical'] else 1)

if __name__ == '__main__':
    main()
