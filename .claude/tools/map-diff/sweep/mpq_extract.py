import ctypes, sys, os
from ctypes import wintypes as w
dll, mpq, outdir = sys.argv[1:4]
os.add_dll_directory(os.path.abspath(dll))
storm = ctypes.WinDLL(os.path.join(dll, 'StormLib.dll')); H = ctypes.c_void_p
storm.SFileOpenArchive.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, ctypes.POINTER(H)]; storm.SFileOpenArchive.restype = ctypes.c_bool
storm.SFileOpenFileEx.argtypes = [H, ctypes.c_char_p, w.DWORD, ctypes.POINTER(H)]; storm.SFileOpenFileEx.restype = ctypes.c_bool
storm.SFileGetFileSize.argtypes = [H, ctypes.POINTER(w.DWORD)]; storm.SFileGetFileSize.restype = w.DWORD
storm.SFileReadFile.argtypes = [H, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD), ctypes.c_void_p]; storm.SFileReadFile.restype = ctypes.c_bool
storm.SFileCloseFile.argtypes = [H]; storm.SFileCloseArchive.argtypes = [H]
def rd(a, name):
    ff = H()
    if not storm.SFileOpenFileEx(a, name.encode('latin1'), 0, ctypes.byref(ff)): return None
    fs = storm.SFileGetFileSize(ff, None); b = ctypes.create_string_buffer(fs + 1); r = ctypes.c_ulong()
    storm.SFileReadFile(ff, b, fs, ctypes.byref(r), None); storm.SFileCloseFile(ff)
    return b.raw[:r.value]
KNOWN = ['war3map.w3e','war3map.wpm','war3map.doo','war3mapUnits.doo','war3map.w3u','war3map.w3t','war3map.w3b','war3map.w3d','war3map.w3a','war3map.w3h','war3map.w3q','war3map.w3i','war3map.wts','war3map.shd','war3map.mmp','war3map.wtg','war3map.wct','war3map.w3r','war3map.w3c','war3map.w3s','war3map.imp','war3mapMisc.txt','war3mapSkin.txt','war3mapExtra.txt','war3mapSkin.w3a','war3mapSkin.w3b','war3mapSkin.w3d','war3mapSkin.w3h','war3mapSkin.w3q','war3mapSkin.w3t','war3mapSkin.w3u','war3map.lua','war3map.j']
a = H()
assert storm.SFileOpenArchive(mpq, 0, 0x100, ctypes.byref(a)), 'open failed'
os.makedirs(outdir, exist_ok=True)
for nm in KNOWN:
    d = rd(a, nm)
    if d is not None:
        open(os.path.join(outdir, nm), 'wb').write(d); print(nm, len(d))
storm.SFileCloseArchive(a)