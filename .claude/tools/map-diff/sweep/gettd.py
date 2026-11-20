import ctypes, sys, os
from ctypes import wintypes as w
dll, out = sys.argv[1:3]
os.add_dll_directory(os.path.abspath(dll))
casc = ctypes.WinDLL(os.path.join(dll, 'CascLib.dll')); H = ctypes.c_void_p
casc.CascOpenStorage.argtypes = [w.LPCWSTR, w.DWORD, ctypes.POINTER(H)]; casc.CascOpenStorage.restype = ctypes.c_bool
casc.CascOpenFile.argtypes = [H, ctypes.c_char_p, w.DWORD, w.DWORD, ctypes.POINTER(H)]; casc.CascOpenFile.restype = ctypes.c_bool
casc.CascGetFileSize.argtypes = [H, ctypes.POINTER(w.DWORD)]; casc.CascGetFileSize.restype = w.DWORD
casc.CascReadFile.argtypes = [H, ctypes.c_void_p, w.DWORD, ctypes.POINTER(w.DWORD)]; casc.CascReadFile.restype = ctypes.c_bool
s = H(); casc.CascOpenStorage(os.environ.get('WC3_INSTALL', 'C:/Program Files (x86)/Warcraft III') + '/:w3', 0xFFFFFFFF, ctypes.byref(s))
fh = H(); assert casc.CascOpenFile(s, b'war3.w3mod:ui' + bytes([92]) + b'triggerdata.txt', 0, 0, ctypes.byref(fh))
sz = casc.CascGetFileSize(fh, None); buf = ctypes.create_string_buffer(sz); got = w.DWORD()
casc.CascReadFile(fh, buf, sz, ctypes.byref(got)); open(out, 'wb').write(buf.raw[:got.value]); print(got.value)
