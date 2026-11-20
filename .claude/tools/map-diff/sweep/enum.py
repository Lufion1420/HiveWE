import ctypes, sys, os, json
from ctypes import wintypes as w
dll = sys.argv[1]; out = sys.argv[2]; mask = sys.argv[3]
os.add_dll_directory(os.path.abspath(dll))
casc = ctypes.WinDLL(os.path.join(dll, 'CascLib.dll'))
H = ctypes.c_void_p
casc.CascOpenStorage.argtypes = [w.LPCWSTR, w.DWORD, ctypes.POINTER(H)]; casc.CascOpenStorage.restype = ctypes.c_bool
casc.CascFindFirstFile.argtypes = [H, ctypes.c_char_p, ctypes.c_void_p, w.LPCWSTR]; casc.CascFindFirstFile.restype = H
casc.CascFindNextFile.argtypes = [H, ctypes.c_void_p]; casc.CascFindNextFile.restype = ctypes.c_bool
s = H()
assert casc.CascOpenStorage(os.environ.get('WC3_INSTALL', 'C:/Program Files (x86)/Warcraft III') + '/:w3', 0xFFFFFFFF, ctypes.byref(s))
buf = ctypes.create_string_buffer(2048)
h = casc.CascFindFirstFile(s, mask.encode(), buf, '')
names = []
if h not in (None, ctypes.c_void_p(-1).value):
    names.append(buf.value.decode('latin1'))
    while casc.CascFindNextFile(h, buf):
        names.append(buf.value.decode('latin1'))
        if len(names) > 400000: break
json.dump(names, open(out, 'w'))
print(len(names), names[:3])
