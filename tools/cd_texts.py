#!/usr/bin/env python3
"""Writes src/game/lang_texts.c: the texts compiled into the CD's German,
French and Italian REUNION.PRG, paired with the English one's.

    tools/cd_texts.py "vendor/Reunion (1995)(Grandslam)(M4).iso"

Each language folder of the CD has an install archive, UNION_1.RNC (an
RNCA archive of RNC ProPack files), whose REUNION.PRG is that language's
build of the floppy release's program: the same code with its texts
translated (docs/versions.md). The English archive's is the floppy's,
uncracked. Texts are paired by aligning the builds' code segment by segment
(objdump, from binutils, disassembles them): a code text is the one the
same instruction loads (mov di,offset; push cs), a typed constant the one
at the same field of the data segment (src/game/dsfields.c, dseg.h).
Prints what it could not pair; src/game/lang.c handles those by hand.
"""
import bisect
import collections
import difflib
import itertools
import os
import re
import struct
import subprocess
import sys
import tempfile

LANGS = ('DEU', 'FRA', 'ITA')
# English code sites (seg:off as the port numbers them) where a build's
# instructions line up with the English but do something else: left
# unpaired there. The Italian planet name line is built in another order
# (planetinfo.c).
DIFFERENT = {
    'ITA': {(0x357b, o) for o in (0x0370, 0x0387, 0x03b4, 0x03e4, 0x041d, 0x0464)},
}
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# --- ISO 9660 ---------------------------------------------------------------


def iso_file(iso, path):
    """The bytes of `path` ('REUNION.DEU/UNION_1.RNC') on a plain ISO 9660 image."""
    with open(iso, 'rb') as f:
        f.seek(16 * 2048)
        pvd = f.read(2048)
        block = struct.unpack('<H', pvd[128:130])[0]
        extent, size = struct.unpack('<I', pvd[158:162])[0], struct.unpack('<I', pvd[166:170])[0]
        for part in path.split('/'):
            f.seek(extent * block)
            d = f.read(size)
            pos, found = 0, None
            while pos < len(d):
                n = d[pos]
                if n == 0:
                    pos = (pos // block + 1) * block
                    continue
                name = d[pos + 33:pos + 33 + d[pos + 32]].decode('latin1').split(';')[0].rstrip('.')
                if name.upper() == part.upper():
                    found = struct.unpack('<I', d[pos + 2:pos + 6])[0], struct.unpack('<I', d[pos + 10:pos + 14])[0]
                    break
                pos += n
            if not found:
                sys.exit('%s: no %s' % (iso, path))
            extent, size = found
        f.seek(extent * block)
        return f.read(size)

# --- RNCA archives and RNC ProPack --------------------------------------------


def rnc_unpack(d):
    """An RNC file: method 0 is stored, method 1 packed."""
    if d[:4] == b'RNC\0':
        return d[8:8 + struct.unpack('>I', d[4:8])[0]]
    assert d[:4] == b'RNC\x01', d[:4]
    usize, psize = struct.unpack('>II', d[4:12])
    src = d[18:18 + psize] + b'\0' * 8
    st = {'buf': src[0] | src[1] << 8, 'cnt': 16, 'p': 0}

    def word(q):
        return src[q] | src[q + 1] << 8

    def advance(n):
        st['buf'] >>= n
        st['cnt'] -= n
        if st['cnt'] < 16:
            st['p'] += 2
            st['buf'] |= word(st['p']) << st['cnt']
            st['cnt'] += 16

    def read(mask, n):
        r = st['buf'] & mask
        advance(n)
        return r

    def table(old):
        num = read(0x1F, 5)
        if not num:
            return old
        lens = [read(0xF, 4) for _ in range(num)]
        t, code = [], 0
        for i in range(1, max(lens) + 1):
            for j in range(num):
                if lens[j] == i:
                    t.append((int(format(code, '0%db' % i)[::-1], 2), i, j))
                    code += 1
            code <<= 1
        return t

    def huf(t):
        for code, n, val in t:
            if st['buf'] & ((1 << n) - 1) == code:
                advance(n)
                if val >= 2:
                    v = 1 << (val - 1)
                    return v | read(v - 1, val - 1)
                return val
        raise ValueError('RNC: bad code')

    out = bytearray()
    advance(2)
    raw = dist = length = []
    while len(out) < usize:
        raw, dist, length = table(raw), table(dist), table(length)
        count = read(0xFFFF, 16)
        while True:
            n = huf(raw)
            if n:
                q = st['p']
                out += src[q:q + n]
                st['p'] = q + n
                st['cnt'] -= 16
                st['buf'] &= (1 << st['cnt']) - 1
                st['buf'] |= word(st['p']) << st['cnt']
                st['cnt'] += 16
            count -= 1
            if count <= 0:
                break
            back, n = huf(dist) + 1, huf(length) + 2
            for _ in range(n):
                out.append(out[-back])
    return bytes(out[:usize])


def rnca_file(d, path):
    """`path` ('GRWAR\\REUNION.PRG') of an RNCA archive: "RNCA", the data's
    offset, 2 bytes, then folders (0, next folder's offset, "DIR\\" 0; the
    root's has no leading 0) of files (name 0, offset)."""
    assert d[:4] == b'RNCA'
    start = struct.unpack('>H', d[4:6])[0]

    def cstr(p):
        j = d.index(b'\0', p)
        return d[p:j].decode('latin1'), j + 1
    folder, pos = cstr(10)
    while pos < start:
        if d[pos] == 0:
            folder, pos = cstr(pos + 3)
            continue
        name, pos = cstr(pos)
        off = struct.unpack('>I', d[pos:pos + 4])[0]
        pos += 4
        if (folder + name).upper() == path.upper():
            return rnc_unpack(d[off:])
    sys.exit('no %s in the archive' % path)

# --- The program --------------------------------------------------------------


class Exe:
    def __init__(self, data):
        h = struct.unpack('<H', data[8:10])[0] * 16
        nrel, relo = struct.unpack('<H', data[6:8])[0], struct.unpack('<H', data[0x18:0x1a])[0]
        self.img = data[h:]
        segs = {0}
        for i in range(nrel):
            off, seg = struct.unpack('<HH', data[relo + 4 * i:relo + 4 * i + 4])
            segs.add(struct.unpack('<H', self.img[seg * 16 + off:seg * 16 + off + 2])[0])
        self.segs = sorted(segs)
        self.ds = self.segs[-1]

    def pstr(self, pos):
        return self.img[pos + 1:pos + 1 + self.img[pos]]


def disasm(img, with_offsets=False):
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(img)
    out = subprocess.run(['objdump', '-D', '-b', 'binary', '-m', 'i8086', f.name],
                         capture_output=True, text=True, check=True).stdout
    os.unlink(f.name)
    found = re.findall(r'^\s*([0-9a-f]+):\t[0-9a-f ]+\t(.*)$', out, re.M)
    ins = [t.strip() for _, t in found]
    return (ins, [int(o, 16) for o, _ in found]) if with_offsets else ins


NUM = re.compile(r'(\$?-?0x[0-9a-f]+|\$?\b\d+\b)')


def pair_code(e, g):
    """CS texts loaded by the same instruction ((segment index, English
    code offset) -> (English text offset, other text offset)), and the DS
    offsets the same instructions use (English, other) -> count."""
    cs, ds = {}, collections.Counter()
    for k in range(len(e.segs) - 1):
        ie, offs = disasm(e.img[e.segs[k] * 16:e.segs[k + 1] * 16], with_offsets=True)
        ig = disasm(g.img[g.segs[k] * 16:g.segs[k + 1] * 16])
        sm = difflib.SequenceMatcher(None, [NUM.sub('#', t) for t in ie],
                                     [NUM.sub('#', t) for t in ig], autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag != 'equal':
                continue
            for i, j in zip(range(i1, i2), range(j1, j2)):
                te, tg = ie[i], ig[j]
                ne, ng = NUM.findall(te), NUM.findall(tg)
                nxt = ie[i + 1] if i + 1 < len(ie) else ''
                if te.startswith('mov') and te.endswith(',%di') and ne and ne[0].startswith('$'):
                    ve, vg = int(ne[0][1:], 16), int(ng[0][1:], 16)
                    if nxt.startswith('push   %cs'):
                        cs[(k, offs[i])] = (ve, vg)
                    elif nxt.startswith('push   %ds'):
                        ds[(ve, vg)] += 1
                elif ne and not te.startswith(('j', 'call', 'lcall', 'ljmp', 'ret', 'lret', 'enter', 'int')):
                    for a, b in zip(ne, ng):
                        if not a.startswith('$') and a.startswith(('0x', '-0x')):
                            va = int(a, 16) & 0xffff
                            if va >= 0x100:
                                ds[(va, int(b, 16) & 0xffff)] += 1
    return cs, ds

# --- The port's typed constants ----------------------------------------------


def ds_slots():
    """(original DS offset, capacity, C expression) of every string slot of DS."""
    fc = open(os.path.join(ROOT, 'src/game/dsfields.c')).read()
    hdr = open(os.path.join(ROOT, 'src/game/dseg.h')).read()
    tables = {m.group(1): m.group(2) for m in
              re.finditer(r'static const DsField fields_(\w+)\[\] = \{(.*?)\n\};', fc, re.S)}
    tables['DSeg'] = re.search(r'const DsField ds_fields\[\] = \{(.*?)\n\};', fc, re.S).group(1)
    recs = re.findall(r'\{"(\w+)", \d+, sizeof\(\w+\), \d+, fields_\w+\}', fc)
    ent = re.compile(r'\{"(\w+)", 0x([0-9A-Fa-f]+), offsetof\([^)]*\), (\d+), (\d+), (-?\d+), '
                     r'[^,{]+, \{(\d+), (\d+), (\d+)\}\}')
    fields = {k: [(n, int(o, 16), int(el), int(c), int(r), (int(a), int(b), int(cc)))
                  for n, o, el, c, r, a, b, cc in ent.findall(v)] for k, v in tables.items()}
    types = {}
    for m in re.finditer(r'typedef struct (\w+) \{(.*?)\n\} (\w+);', hdr, re.S):
        types[m.group(3)] = dict((f, t) for t, f in
                                 re.findall(r'^\s+(PStr\d+|PString)\s+(\w+)', m.group(2), re.M))
    out = []

    def walk(table, base, expr):
        for name, ofs, elem, count, rec, dims in fields[table]:
            t = types.get(table, {}).get(name)
            sizes = [d for d in dims if d > 1]
            for k in range(count):
                ix, r = [], k
                for d in reversed(sizes):
                    ix.append(r % d)
                    r //= d
                e = expr + name + ''.join('[%d]' % i for i in reversed(ix))
                at = base + ofs + k * elem
                if t:
                    out.append((at, 255 if t == 'PString' else int(t[4:]), e))
                elif rec >= 0:
                    walk(recs[rec], at, e + '.')
    walk('DSeg', 0, 'ds.')
    return out


TEXT = set(range(0x20, 0x7f)) | set(range(0x80, 0x9b)) | {0xe1}


def pair_ds(e, g, refs, slots):
    """Each DS slot's text in the other build: one shift and element stride
    per array, the one under which most slots read as text where the English
    one has text."""
    de, dg = e.img[e.ds * 16:], g.img[g.ds * 16:]

    def valid(p, cap):
        return 0 <= p < len(dg) and dg[p] <= cap and all(c in TEXT for c in dg[p + 1:p + 1 + dg[p]])
    sm = difflib.SequenceMatcher(None, de, dg, autojunk=False)
    blocks = [(a, b - a) for a, b, n in sm.get_matching_blocks() if n >= 4]
    starts = [a for a, _ in blocks]
    refsh = collections.defaultdict(collections.Counter)
    for (va, vb), n in refs.items():
        if va < len(de):
            refsh[va][vb - va] += n
    groups = collections.OrderedDict()
    for at, cap, expr in slots:
        if at < len(de):
            groups.setdefault(re.sub(r'\[\d+\](\.\w+)?$', '', expr), []).append((at, cap, expr))
    out = {}
    for ss in groups.values():
        lo, hi = ss[0][0], ss[-1][0] + ss[-1][1] + 1
        cands = collections.Counter()
        for va in refsh:
            if lo - 64 <= va < hi:
                cands.update(refsh[va])
        for at, _, _ in ss:
            i = bisect.bisect_right(starts, at) - 1
            cands[blocks[i][1] if i >= 0 else 0] += 0
        stride = ss[1][0] - ss[0][0] if len(ss) > 1 else 0
        best, best_score = (0, stride), -1
        for sh in cands:
            for d in ([0] if not stride else range(-2, 9)):
                score = 0
                for k, (at, cap, _) in enumerate(ss):
                    p = ss[0][0] + sh + k * (stride + d)
                    has = de[at] > 0
                    score += valid(p, cap + 8) and (dg[p] > 0) == has
                if score > best_score or (score == best_score and d == 0 and best[1] != stride):
                    best, best_score = (sh, stride + d), score
        sh, st = best
        for k, (at, cap, expr) in enumerate(ss):
            p = ss[0][0] + sh + k * st
            out[expr] = dg[p + 1:p + 1 + dg[p]] if valid(p, 255) else None
    return out

# --- Output -------------------------------------------------------------------


def c_str(b):
    """A C string literal for CP437 bytes; octal escapes cannot swallow the
    characters after them."""
    if b is None:
        return 'nullptr'
    s = ''
    for c in b:
        if c in (0x22, 0x5c):
            s += '\\' + chr(c)
        elif 0x20 <= c < 0x7f:
            s += chr(c)
        else:
            s += '\\%03o' % c
    return '"%s"' % s


def main(iso):
    progs = {L: Exe(rnca_file(iso_file(iso, 'REUNION.%s/UNION_1.RNC' % L), 'GRWAR\\REUNION.PRG'))
             for L in ('ENG',) + LANGS}
    e = progs['ENG']
    slots = ds_slots()
    cs_tr = collections.defaultdict(dict)   # (seg index, code off) -> {lang: text}
    english = {}                            # (seg index, code off) -> English text
    ds_tr = collections.defaultdict(dict)   # expr -> {lang: text}
    for L in LANGS:
        g = progs[L]
        cs, refs = pair_code(e, g)
        for (k, site), (v, w) in cs.items():
            if (e.segs[k] + 0x1000, site) in DIFFERENT.get(L, ()):
                continue
            english[(k, site)] = e.pstr(e.segs[k] * 16 + v)
            cs_tr[(k, site)][L] = g.pstr(g.segs[k] * 16 + w)
        for expr, t in pair_ds(e, g, refs, slots).items():
            ds_tr[expr][L] = t
        print('%s: %d code texts paired' % (L, len(cs)))
    by_text = collections.defaultdict(list)
    for key, en in english.items():
        by_text[en].append(key)

    def row(en, tr):
        return tuple(tr.get(L) for L in LANGS)
    def text(b):
        return b is None or all(c in TEXT for c in b)
    plain, at = [], []
    for en, keys in sorted(by_text.items()):
        if not text(en) or not all(text(t) for k in keys for t in cs_tr[k].values()):
            continue  # code read as a text
        rows = {row(en, cs_tr[k]) for k in keys}
        if all(t in (None, en) for r in rows for t in r):
            continue
        if len(rows) == 1:
            plain.append((en, rows.pop()))
        else:
            for k in sorted(keys):
                r = row(en, cs_tr[k])
                if any(t not in (None, en) for t in r):
                    at.append(((e.segs[k[0]] + 0x1000) << 16 | k[1], en, r))
    ds_rows = []
    caps = {expr: cap for _, cap, expr in slots}
    for at_, cap, expr in slots:
        if expr not in ds_tr:
            continue
        en = e.img[e.ds * 16 + at_ + 1:e.ds * 16 + at_ + 1 + e.img[e.ds * 16 + at_]]
        r = tuple(ds_tr[expr].get(L) for L in LANGS)
        if all(t in (None, en) for t in r):
            continue
        ds_rows.append((expr, en, r))

    path = os.path.join(ROOT, 'src/game/lang_texts.c')
    with open(path, 'w') as f:
        f.write('// Generated by tools/cd_texts.py from the CD image; do not edit.\n'
                '// The texts of the German, French and Italian REUNION.PRG paired with\n'
                '// the English one\'s (game/lang.h).\n'
                '#include "game/lang.h"\n\n#include "game/dseg.h"\n\n')
        f.write('const LangText lang_texts[] = {\n')
        for en, r in plain:
            f.write('    {%s, {%s}},\n' % (c_str(en), ', '.join(c_str(t) for t in r)))
        f.write('};\nconst int lang_text_count = (int)(sizeof lang_texts / sizeof lang_texts[0]);\n\n')
        f.write('const LangTextAt lang_texts_at[] = {\n')
        for addr, en, r in sorted(at):
            f.write('    {0x%08x, %s, {%s}},\n' % (addr, c_str(en), ', '.join(c_str(t) for t in r)))
        f.write('};\nconst int lang_text_at_count = (int)(sizeof lang_texts_at / sizeof lang_texts_at[0]);\n\n')
        f.write('#define SLOT(f) &(f).len, (uint8_t)sizeof (f).s\n\n')
        f.write('const LangSlot lang_slots[] = {\n')
        for expr, en, r in ds_rows:
            f.write('    {SLOT(%s), {%s}},\n' % (expr, ', '.join(c_str(t) for t in r)))
        f.write('};\nconst int lang_slot_count = (int)(sizeof lang_slots / sizeof lang_slots[0]);\n')
    print('%s: %d code texts, %d by address, %d typed constants' % (path, len(plain), len(at), len(ds_rows)))
    for addr, en, r in sorted(at):
        print('by address %04x:%04x %r -> %r' % (addr >> 16, addr & 0xffff, en, r))
    # What was not paired: every text the English code loads.
    loads = {}
    for k in range(len(e.segs) - 1):
        ins, offs = disasm(e.img[e.segs[k] * 16:e.segs[k + 1] * 16], with_offsets=True)
        for i in range(len(ins) - 1):
            n = NUM.findall(ins[i])
            if (ins[i].startswith('mov') and ins[i].endswith(',%di') and n and n[0].startswith('$')
                    and ins[i + 1].startswith('push   %cs')):
                loads[(k, offs[i])] = int(n[0][1:], 16)
    for L in LANGS:
        for key in sorted(loads):
            if L not in cs_tr.get(key, {}) and (e.segs[key[0]] + 0x1000, key[1]) not in DIFFERENT.get(L, ()):
                print('%s: unpaired %04x:%04x %r' % (
                    L, e.segs[key[0]] + 0x1000, key[1], e.pstr(e.segs[key[0]] * 16 + loads[key])))
        for expr, en, r in ds_rows:
            t = r[LANGS.index(L)]
            if t is not None and len(t) > caps[expr]:
                print('%s: %s %r is longer than its slot (%d)' % (L, expr, t, caps[expr]))


if __name__ == '__main__':
    main(sys.argv[1])
