"""Extract war3map.lua / war3map.j of the stock maps from CASC (read-only).

usage: extract_scripts.py <dll_dir> <stock_root> <out_root> [wc3_install]
  dll_dir    folder with CascLib.dll and StormLib.dll. Use a COPY in your scratchpad,
             not build/Agent/Release: loading them from there locks files another
             agent's build needs.
  stock_root folder with <id>/war3map.w3i and index.json {id: "war3.w3mod:...\\map.w3x"}
Stock maps are MPQ archives stored inside CASC. For each map the script reads the
archive (the localized copy under _locales\\enus.w3mod first, then the base copy),
opens it with StormLib and copies the script out. Writes:
  <out_root>/<id>/war3map.lua|war3map.j
  <out_root>/all.json, subset.json, subset.tsv   (subset = w3i version 39, i.e. 3.0 maps)
"""
import ctypes, sys, os, json, struct
from collections import Counter
from ctypes import wintypes as w

BS = chr(92)
dll_dir, stock, out = sys.argv[1], sys.argv[2], sys.argv[3]
install = sys.argv[4] if len(sys.argv) > 4 else 'C:/Program Files (x86)/Warcraft III'
os.add_dll_directory(os.path.abspath(dll_dir))
casc = ctypes.WinDLL(os.path.join(dll_dir, 'CascLib.dll'))
storm = ctypes.WinDLL(os.path.join(dll_dir, 'StormLib.dll'))
H = ctypes.c_void_p
casc.CascOpenStorage.argtypes = [w.LPCWSTR, w.DWORD, ctypes.POINTER(H)]; casc.CascOpenStorage.restype = ctypes.c_bool
casc.CascOpenFile.argtypes = [H, ctypes.c_char_p, w.DWORD, w.DWORD, ctypes.POINTER(H)]; casc.CascOpenFile.restype = ctypes.c_bool
casc.CascGetFileSize.argtypes = [H, ctypes.POINTER(w.DWORD)]; casc.CascGetFileSize.restype = w.DWORD
casc.CascReadFile.argtypes = [H, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD)]; casc.CascReadFile.restype = ctypes.c_bool
casc.CascCloseFile.argtypes = [H]
storm.SFileOpenArchive.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.POINTER(H)]; storm.SFileOpenArchive.restype = ctypes.c_bool
storm.SFileOpenFileEx.argtypes = [H, ctypes.c_char_p, w.DWORD, ctypes.POINTER(H)]; storm.SFileOpenFileEx.restype = ctypes.c_bool
storm.SFileGetFileSize.argtypes = [H, ctypes.POINTER(w.DWORD)]; storm.SFileGetFileSize.restype = w.DWORD
storm.SFileReadFile.argtypes = [H, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD), ctypes.c_void_p]; storm.SFileReadFile.restype = ctypes.c_bool
storm.SFileCloseFile.argtypes = [H]; storm.SFileCloseArchive.argtypes = [H]

s = H()
if not casc.CascOpenStorage(install + '/:w3', 0xFFFFFFFF, ctypes.byref(s)):
    sys.exit('CascOpenStorage failed %d' % ctypes.GetLastError())

def casc_read(name):
    fh = H()
    if not casc.CascOpenFile(s, name.encode('latin1'), 0, 0, ctypes.byref(fh)):
        return None
    sz = casc.CascGetFileSize(fh, None)
    if sz == 0xFFFFFFFF:  # CASC_INVALID_SIZE
        casc.CascCloseFile(fh); return None
    buf = ctypes.create_string_buffer(sz); got = w.DWORD()
    ok = casc.CascReadFile(fh, buf, sz, ctypes.byref(got)); casc.CascCloseFile(fh)
    return buf.raw[:got.value] if ok else None

def mpq_read(a, name):
    ff = H()
    if not storm.SFileOpenFileEx(a, name.encode('latin1'), 0, ctypes.byref(ff)):
        return None
    fs = storm.SFileGetFileSize(ff, None); b = ctypes.create_string_buffer(fs + 1); r = w.DWORD()
    storm.SFileReadFile(ff, b, fs, ctypes.byref(r), None); storm.SFileCloseFile(ff)
    return b.raw[:r.value]

SCRIPTS = (('war3map.lua', 'Lua'), ('war3map.j', 'JASS'), ('scripts' + BS + 'war3map.j', 'JASS'))
index = json.load(open(os.path.join(stock, 'index.json')))
os.makedirs(out, exist_ok=True)
tmp = os.path.join(out, '_tmp.mpq')
rows = []
for mid, cpath in sorted(index.items()):
    w3i = os.path.join(stock, mid, 'war3map.w3i')
    ver = struct.unpack('<I', open(w3i, 'rb').read(4))[0] if os.path.exists(w3i) else None
    mod, rel = cpath.split(':', 1)
    row = {'map': mid, 'casc': cpath, 'w3i_version': ver, 'language': None, 'script': None, 'archive': None}
    for arc in (mod + ':_locales' + BS + 'enus.w3mod:' + rel, cpath):
        data = casc_read(arc)
        if not data: continue
        open(tmp, 'wb').write(data); del data
        a = H()
        if not storm.SFileOpenArchive(tmp, 0, 0x100, ctypes.byref(a)): continue
        for fname, lang in SCRIPTS:
            sc = mpq_read(a, fname)
            if sc:
                d = os.path.join(out, mid); os.makedirs(d, exist_ok=True)
                p = os.path.join(d, fname.split(BS)[-1]); open(p, 'wb').write(sc)
                row.update(language=lang, script=p, archive=arc); break
        storm.SFileCloseArchive(a)
        if row['language']: break
    rows.append(row)
    print(mid, ver, row['language'] or 'MISSING', row['archive'] or '', flush=True)
if os.path.exists(tmp): os.remove(tmp)

json.dump(rows, open(os.path.join(out, 'all.json'), 'w'), indent=1)
sub = [r for r in rows if r['w3i_version'] == 39]
json.dump(sub, open(os.path.join(out, 'subset.json'), 'w'), indent=1)
with open(os.path.join(out, 'subset.tsv'), 'w') as f:
    f.write('map\tw3i_version\tlanguage\tscript\tcasc\n')
    for r in sub:
        f.write('\t'.join(str(r[k]) for k in ('map', 'w3i_version', 'language', 'script', 'casc')) + '\n')
print('all:', dict(Counter(r['language'] for r in rows)))
print('v39:', len(sub), dict(Counter(r['language'] for r in sub)))
