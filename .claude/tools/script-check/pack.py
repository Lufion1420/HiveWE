import ctypes, sys, os, struct
from ctypes import wintypes as W
dll=ctypes.WinDLL(sys.argv[1]); src=sys.argv[2]; out=sys.argv[3]
dll.SFileCreateArchive.argtypes=[ctypes.c_wchar_p,W.DWORD,W.DWORD,ctypes.POINTER(W.HANDLE)]
dll.SFileAddFileEx.argtypes=[W.HANDLE,ctypes.c_wchar_p,ctypes.c_char_p,W.DWORD,W.DWORD,W.DWORD]
dll.SFileCloseArchive.argtypes=[W.HANDLE]
if os.path.exists(out): os.remove(out)
h=W.HANDLE()
assert dll.SFileCreateArchive(out,0,4096,ctypes.byref(h)),'create'
n=0
for r,_,fs in os.walk(src):
    for f in fs:
        p=os.path.join(r,f); nm=os.path.relpath(p,src).replace('/',chr(92))
        if not dll.SFileAddFileEx(h,p,nm.encode('latin1'),0x80000200,0x02,0x02): print('FAIL',nm)
        else: n+=1
dll.SFileCloseArchive(h); print('packed',n,out)
def ver(path,off):
    try: b=open(os.path.join(src,path),'rb').read(); return struct.unpack_from('<I',b,off)[0], (struct.unpack_from('<I',b,off+4)[0] if path.endswith('doo') else None)
    except Exception as e: return str(e)
print('w3i',ver('war3map.w3i',0)); print('w3r',ver('war3map.w3r',0)); print('w3c',ver('war3map.w3c',0))
print('doo (ver,sub)',ver('war3map.doo',4)); print('units (ver,sub)',ver('war3mapUnits.doo',4))

