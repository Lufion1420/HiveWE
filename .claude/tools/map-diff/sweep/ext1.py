"""Extract one stock map: ext1.py <dll> <casc_path> <stock_dir> <scripts_dir>. Known names plus (listfile) entries named war3map*/scripts. Never SFileFindFirstFile."""
import ctypes, sys, os
from ctypes import wintypes as w
BS = chr(92)
dll, cpath, stock, scripts = sys.argv[1:5]
os.add_dll_directory(os.path.abspath(dll))
casc = ctypes.WinDLL(os.path.join(dll, 'CascLib.dll')); storm = ctypes.WinDLL(os.path.join(dll, 'StormLib.dll'))
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
if not casc.CascOpenStorage(os.environ.get('WC3_INSTALL', 'C:/Program Files (x86)/Warcraft III') + '/:w3', 0xFFFFFFFF, ctypes.byref(s)): sys.exit('open failed')
def casc_read(name):
    fh = H()
    if not casc.CascOpenFile(s, name.encode('latin1'), 0, 0, ctypes.byref(fh)): return None
    sz = casc.CascGetFileSize(fh, None)
    if sz == 0xFFFFFFFF: casc.CascCloseFile(fh); return None
    buf = ctypes.create_string_buffer(sz); got = w.DWORD()
    ok = casc.CascReadFile(fh, buf, sz, ctypes.byref(got)); casc.CascCloseFile(fh)
    return buf.raw[:got.value] if ok else None
def mpq_read(a, name):
    ff = H()
    if not storm.SFileOpenFileEx(a, name.encode('latin1'), 0, ctypes.byref(ff)): return None
    fs = storm.SFileGetFileSize(ff, None); b = ctypes.create_string_buffer(fs + 1); r = w.DWORD()
    storm.SFileReadFile(ff, b, fs, ctypes.byref(r), None); storm.SFileCloseFile(ff)
    return b.raw[:r.value]
KNOWN = ['war3map.w3e','war3map.wpm','war3map.doo','war3mapUnits.doo','war3map.w3u','war3map.w3t','war3map.w3b','war3map.w3d','war3map.w3a','war3map.w3h','war3map.w3q','war3map.w3i','war3map.wts','war3map.shd','war3map.mmp','war3map.wtg','war3map.wct','war3map.w3r','war3map.w3c','war3map.w3s','war3map.imp','war3mapMisc.txt','war3mapSkin.txt','war3mapExtra.txt','war3mapSkin.w3a','war3mapSkin.w3b','war3mapSkin.w3d','war3mapSkin.w3h','war3mapSkin.w3q','war3mapSkin.w3t','war3mapSkin.w3u','war3map.w3o','war3map.wai']
SCRIPTS = ['war3map.lua','war3map.j','scripts' + BS + 'war3map.j']
mod, rel = cpath.split(':', 1)
tmp = os.path.join(stock, '_tmp_%d.mpq' % os.getpid())
done = False
for arc in (mod + ':_locales' + BS + 'enus.w3mod:' + rel, cpath):
    data = casc_read(arc)
    if not data: continue
    open(tmp, 'wb').write(data); del data
    a = H()
    if not storm.SFileOpenArchive(tmp, 0, 0x100, ctypes.byref(a)): continue
    names = list(KNOWN)
    lf = mpq_read(a, '(listfile)')
    if lf:
        for ln in lf.decode('latin1').split('\n'):
            ln = ln.strip()
            if ln.lower().startswith('war3map') and ln not in names: names.append(ln)
    n = 0
    for nm in names:
        d = mpq_read(a, nm)
        if d is not None:
            open(os.path.join(stock, nm.replace(BS, '_')), 'wb').write(d); n += 1
    for nm in SCRIPTS:
        d = mpq_read(a, nm)
        if d:
            open(os.path.join(scripts, nm.split(BS)[-1]), 'wb').write(d); break
    storm.SFileCloseArchive(a); done = True; print('ok', arc, n); break
if os.path.exists(tmp): os.remove(tmp)
sys.exit(0 if done else 3)
