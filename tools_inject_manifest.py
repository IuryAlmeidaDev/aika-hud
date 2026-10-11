import ctypes
from ctypes import wintypes
import os

kernel32 = ctypes.windll.kernel32

kernel32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
kernel32.BeginUpdateResourceW.restype = wintypes.HANDLE

kernel32.UpdateResourceW.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.c_void_p,
    wintypes.WORD,
    ctypes.c_void_p,
    wintypes.DWORD
]
kernel32.UpdateResourceW.restype = wintypes.BOOL

kernel32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
kernel32.EndUpdateResourceW.restype = wintypes.BOOL

RT_MANIFEST = 24

MANIFEST_XML = b'''<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity version="1.0.0.0" processorArchitecture="*" name="AikaHUD" type="win32"/>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="requireAdministrator" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
</assembly>
'''

def inject_manifest(exe_path):
    abs_exe = os.path.abspath(exe_path)
    h_update = kernel32.BeginUpdateResourceW(abs_exe, False)
    if not h_update:
        err = kernel32.GetLastError()
        raise RuntimeError(f"BeginUpdateResourceW failed: {err}")

    buf = ctypes.create_string_buffer(MANIFEST_XML)
    res = kernel32.UpdateResourceW(
        h_update,
        ctypes.c_void_p(RT_MANIFEST),
        ctypes.c_void_p(1),
        0,
        ctypes.cast(buf, ctypes.c_void_p),
        len(MANIFEST_XML)
    )
    if not res:
        err = kernel32.GetLastError()
        raise RuntimeError(f"UpdateResourceW RT_MANIFEST failed: {err}")

    if not kernel32.EndUpdateResourceW(h_update, False):
        err = kernel32.GetLastError()
        raise RuntimeError(f"EndUpdateResourceW failed: {err}")

    print(f"[OK] Manifest requireAdministrator embutido com sucesso em {exe_path}")

if __name__ == '__main__':
    inject_manifest('AikaHUD.exe')
