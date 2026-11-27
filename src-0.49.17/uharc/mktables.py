"""Regenerate the compressed UHARC const tables.

One-shot and destructive: it reads the pristine sources from
``tables_original/`` and OVERWRITES the eight generated files in the
target directory (this script's directory by default).  It never reads
the generated files, so it is safe to run repeatedly.

    python mktables.py [target_dir]

The three stages (blob generation, C malloc fix, Init wiring) used during
the port are combined here so a clean checkout can be rebuilt in one step.
"""
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ORIG = os.path.join(HERE, 'tables_original')
DST = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else HERE

SIZE = {'char': 1, 'unsigned char': 1, 'BYTE': 1,
        'short': 2, 'unsigned short': 2, 'WORD': 2,
        'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4,
        'DWORD': 4, '__int64': 8}

# file -> (init function name, exported with C linkage?)
TARGETS = [
    ('uha03_dec4_dg.c', 'uha03_dg_init', True),
    ('uha04_dec4_seed.c', 'uha04_seed_init', True),
    ('UhaDec4_03.cpp', 'xac_uha03_init', False),
    ('UhaDec4_04.cpp', 'xac_uha04_init', False),
    ('UhaDec4_06.cpp', 'xac_uha06_init', False),
    ('UhaDec5.cpp', 'xac_uha05_init', False),
    ('dec4c_rom.h', 'xac_dec4c_rom_init', False),
    ('dec4c_rom2.h', 'xac_dec4c_rom2_init', False),
]

NUM = r'(?:0[xX][0-9a-fA-F]+|\d+)(?:[uUlL]*)'
PAT = re.compile(
    r'(?m)^(?P<lead>(?:static\s+)?const\s+(?P<type>' +
    r'(?:unsigned\s+)?(?:char|BYTE|short|WORD|int|long|DWORD|uint\w*|__int64))\s+' +
    r'(?P<name>\w+)\s*\[(?P<size>\d*)\]\s*=\s*\{)(?P<body>[^{}]*)\};')


def generate(fname, initname, exported):
    raw = open(os.path.join(ORIG, fname), 'rb').read()
    text = raw.decode('latin1')
    nl = '\r\n' if raw.count(b'\r\n') > raw.count(b'\n') / 2 else '\n'
    rows, pieces, out, last = [], [], [], 0
    for m in PAT.finditer(text):
        t = ' '.join(m.group('type').split())
        toks = [x.rstrip('uUlL') for x in re.findall(NUM, m.group('body'))]
        if not toks:
            continue
        n = int(m.group('size')) if m.group('size') else len(toks)
        assert len(toks) >= n, (fname, m.group('name'), len(toks), n)
        toks = toks[:n]
        unit = SIZE.get(t, 4)
        if unit == 1:
            b = bytes(int(x, 0) & 0xFF for x in toks)
        elif unit == 2:
            b = b''.join(int(x, 0).to_bytes(2, 'little', signed=False) for x in toks)
        else:
            b = b''.join(int(x, 0).to_bytes(4, 'little', signed=False) for x in toks)
        assert len(b) == unit * n
        rows.append((m.group('name'), t, n, len(b)))
        pieces.append(b)
        out.append(text[last:m.start()])
        last = m.end()
    assert rows, fname
    out.append(text[last:])
    text = ''.join(out)

    allbytes = b''.join(pieces)
    co = zlib.compressobj(6, zlib.DEFLATED, -15)
    blob = co.compress(allbytes) + co.flush()
    assert zlib.decompress(blob, -15) == allbytes

    L = ['/* Generated: const tables of %s compressed with raw deflate (level 6).' % fname,
         '   Total %d bytes -> %d bytes.  Do not edit by hand. */' %
         (len(allbytes), len(blob)),
         '#ifndef XAC_ZLIB_H_INCLUDED', '#include "../zlib/zlib.h"', '#endif',
         '#include <string.h>']
    if fname.endswith('.c'):
        L.append('#include <stdlib.h>')
    for name, t, n, nb in rows:
        L.append('%s%s %s[%d];' % ('' if exported else 'static ', t, name, n))
    L.append('static const unsigned char %s_blob[%d] = {' % (initname, len(blob)))
    for i in range(0, len(blob), 16):
        L.append('  ' + ','.join('0x%02x' % x for x in blob[i:i+16]) + ',')
    L.append('};')
    L.append(('void %s(void) {' if exported else 'static void %s(void) {') % initname)
    L.append('  static int done = 0;')
    L.append('  if (done) return;')
    if fname.endswith('.c'):
        L.append('  { unsigned char* buf = (unsigned char*)malloc(%d);' % len(allbytes))
    else:
        L.append('  { unsigned char* buf = new unsigned char[%d];' % len(allbytes))
    L.append('    z_stream s; memset(&s, 0, sizeof(s));')
    L.append('    s.next_in = (Bytef*)%s_blob;' % initname)
    L.append('    s.avail_in = (uInt)sizeof(%s_blob);' % initname)
    L.append('    s.next_out = buf; s.avail_out = %d;' % len(allbytes))
    cleanup = 'free(buf)' if fname.endswith('.c') else 'delete [] buf'
    L.append('    if (Z_OK != inflateInit2(&s, -15) ||')
    L.append('        Z_STREAM_END != inflate(&s, Z_FINISH) ||')
    L.append('        s.total_out != %d) { inflateEnd(&s); %s; return; }' %
             (len(allbytes), cleanup))
    L.append('    inflateEnd(&s);')
    off = 0
    for name, t, n, nb in rows:
        L.append('    memcpy(%s, buf + %d, %d);' % (name, off, nb))
        off += nb
    L.append('    %s;' % cleanup)
    L.append('  }')
    L.append('  done = 1;')
    L.append('}')
    L.append('')
    block = nl.join(L) + nl

    if fname.endswith('.h'):
        marker = '#ifndef ' + ('DEC4C_ROM_H' if fname == 'dec4c_rom.h' else 'DEC4C_ROM2_H')
        i = text.find(marker)
        assert i >= 0, fname
        j = text.find(nl, i) + len(nl)
        text = text[:j] + block + text[j:]
    else:
        # insert by byte position, preserving every untouched line ending
        incs = list(re.finditer(r'(?m)^#include[^\r\n]*\r?\n', text[:4000]))
        if incs:
            pos = incs[-1].end()
        else:
            c = text[:4000].find('*/')
            assert c >= 0, fname
            pos = c + 2
            while pos < len(text) and text[pos] in '\r\n':
                pos += 1
        text = text[:pos] + block + text[pos:]

    outpath = os.path.join(DST, fname)
    open(outpath, 'wb').write(text.encode('latin1'))
    print('%-22s %4d arrays %7d -> %6d bytes' %
          (fname, len(rows), len(allbytes), len(blob)))


def wire_inits():
    pairs = {
        'UhaDec4_03.cpp': [
            ('extern const unsigned char d3u_dgroup[];',
             'extern unsigned char d3u_dgroup[];\nextern "C" void uha03_dg_init(void);'),
            ('int UhaDec4_03Init(void **pctx, int method)\n{',
             'int UhaDec4_03Init(void **pctx, int method)\n{\n'
             '  uha03_dg_init();\n  xac_uha03_init();')],
        'UhaDec4_04.cpp': [
            ('extern "C" const unsigned char d4v_seed_436100[512];\n'
             'extern "C" const unsigned char d4v_price_seed[34458];',
             'extern "C" unsigned char d4v_seed_436100[512];\n'
             'extern "C" unsigned char d4v_price_seed[34458];\n'
             'extern "C" void uha04_seed_init(void);'),
            ('int UhaDec4_04Init(void **pctx, int method)\n{',
             'int UhaDec4_04Init(void **pctx, int method)\n{\n'
             '  uha04_seed_init();\n  xac_uha04_init();')],
        'UhaDec4_06.cpp': [
            ('int UhaDec4_06Init(void **ctx, int method)\n{',
             'int UhaDec4_06Init(void **ctx, int method)\n{\n'
             '  xac_dec4c_rom_init();\n  xac_dec4c_rom2_init();\n  xac_uha06_init();')],
        'UhaDec5.cpp': [
            ('int UhaDec5_%sInit(void** pctx, int method)\n{' % v,
             'int UhaDec5_%sInit(void** pctx, int method)\n{\n  xac_uha05_init();' % v)
            for v in ('06', '04', '03')],
    }
    for fname, edits in pairs.items():
        p = os.path.join(DST, fname)
        d = open(p, 'rb').read()
        for old, new in edits:
            ob, nb = old.encode('ascii'), new.encode('ascii')
            assert d.count(ob) == 1, (fname, old[:60], d.count(ob))
            d = d.replace(ob, nb)
        open(p, 'wb').write(d)
        print('wired', fname)


for f, initname, exported in TARGETS:
    generate(f, initname, exported)
wire_inits()
