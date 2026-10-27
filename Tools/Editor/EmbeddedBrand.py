"""Admit actual PE brand resources as data, without executing a DLL or using Pillow."""
from pathlib import Path
import ctypes
from ctypes import wintypes
import struct

def require_embedded_brand(binary):
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.LoadLibraryExW.argtypes=(wintypes.LPCWSTR,wintypes.HANDLE,wintypes.DWORD);k.LoadLibraryExW.restype=wintypes.HMODULE
    k.FindResourceW.argtypes=(wintypes.HMODULE,wintypes.LPCWSTR,wintypes.LPCWSTR);k.FindResourceW.restype=wintypes.HRSRC
    k.SizeofResource.argtypes=(wintypes.HMODULE,wintypes.HRSRC);k.SizeofResource.restype=wintypes.DWORD
    k.LoadResource.argtypes=(wintypes.HMODULE,wintypes.HRSRC);k.LoadResource.restype=wintypes.HGLOBAL
    k.LockResource.argtypes=(wintypes.HGLOBAL,);k.LockResource.restype=ctypes.c_void_p
    k.FreeLibrary.argtypes=(wintypes.HMODULE,)
    module=k.LoadLibraryExW(str(Path(binary).resolve()),None,0x22)
    if not module:raise ctypes.WinError(ctypes.get_last_error())
    def resource(id,kind):
        name=ctypes.cast(ctypes.c_void_p(id),wintypes.LPCWSTR);kind=ctypes.cast(ctypes.c_void_p(kind),wintypes.LPCWSTR)
        found=k.FindResourceW(module,name,kind)
        if not found:raise RuntimeError('Missing embedded branding')
        size=k.SizeofResource(module,found);data=k.LockResource(k.LoadResource(module,found))
        if not size or not data:raise RuntimeError('Empty brand resource')
        return ctypes.string_at(data,size)
    try:
        brand=Path(__file__).resolve().parents[2]/'Assets/Brand/Export'
        if resource(102,10)!=(brand/'nexora-wordmark.png').read_bytes():raise RuntimeError('Embedded wordmark mismatch')
        group=resource(101,14)
        if struct.unpack_from('<HHH',group)!=(0,1,7):raise RuntimeError('Icon group count')
        sizes=set()
        for i in range(7):
            w,h,_,_,planes,bits,n,id=struct.unpack_from('<BBBBHHIH',group,6+14*i)
            w,h=w or 256,h or 256
            if w!=h or w in sizes or (planes,bits)!=(1,32):raise RuntimeError('Invalid icon directory')
            data=resource(id,3)
            if len(data)!=n or data!=(brand/f'nexora-editor-icon-{w}.png').read_bytes():raise RuntimeError('Embedded icon mismatch')
            sizes.add(w)
        if sizes!={16,24,32,48,64,128,256}:raise RuntimeError('Missing icon size')
    finally:k.FreeLibrary(module)
