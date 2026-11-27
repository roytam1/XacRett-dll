"""Regenerate dictionary_blob.h from dictionary_inc.h (RFC 7932 dictionary).

    python mkdictionary.py

Raw deflate is required: XacRett patches zlib MAX_WBITS to 16 for
Deflate64, so uncompress()/inflateInit() cannot read a zlib wrapper.
The generated header is consumed by ZipTool.cpp, which inflates it once
and calls BrotliSetDictionaryData().
"""
import os
import re
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, 'dictionary_inc.h')).read()
body = src.split('{', 1)[1].rsplit('}', 1)[0]
raw = bytes(int(x) for x in re.findall(r'\d+', body))
assert len(raw) == 122784, len(raw)

co = zlib.compressobj(6, zlib.DEFLATED, -15)
blob = co.compress(raw) + co.flush()
assert zlib.decompress(blob, -15) == raw

L = ['/* Generated from dictionary_inc.h (RFC 7932 static dictionary, 122784 B)',
     '   with zlib level 6, RAW deflate (wbits=-15).  Do not edit by hand.',
     '   Regenerate with the tracked mkdictionary.py. */',
     '#ifndef AFX_BROTLI_DICTIONARY_BLOB_H__INCLUDED__',
     '#define AFX_BROTLI_DICTIONARY_BLOB_H__INCLUDED__',
     '',
     '#define XAC_BROTLI_DICT_SIZE 122784',
     'static const unsigned char kXacBrotliDictionaryZ[%d] = {' % len(blob)]
for i in range(0, len(blob), 20):
    L.append('  ' + ','.join('0x%02x' % b for b in blob[i:i+20]) + ',')
L += ['};', '', '#endif']
open(os.path.join(HERE, 'dictionary_blob.h'), 'w', newline='\n').write('\n'.join(L) + '\n')
print('raw %d -> blob %d bytes' % (len(raw), len(blob)))
