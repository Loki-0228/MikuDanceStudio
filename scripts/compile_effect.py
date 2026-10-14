"""Compile embedded D3DX9 effects at build time, without creating a D3D device."""
import ctypes as ct
from functools import lru_cache


@lru_cache(maxsize=3)
def compile_effect(source):
    # Use the same compiler and flags as runtime effect creation. Bytecode is
    # architecture independent; never load a project-local DLL during a build.
    system = ct.create_unicode_buffer(32768)
    kernel = ct.WinDLL('kernel32', use_last_error=True)
    if not kernel.GetSystemDirectoryW(system, len(system)):
        raise ct.WinError(ct.get_last_error())
    dll = ct.WinDLL(system.value + r'\d3dx9_43.dll')
    ptr = ct.c_void_p
    create = dll.D3DXCreateEffectCompiler
    create.argtypes = [ptr, ct.c_uint, ptr, ptr, ct.c_uint, ct.POINTER(ptr), ct.POINTER(ptr)]
    create.restype = ct.c_long

    def method(obj, slot, result, *args):
        table = ct.cast(obj, ct.POINTER(ct.POINTER(ptr))).contents
        return ct.WINFUNCTYPE(result, ptr, *args)(table[slot])

    def data(obj):
        address = method(obj, 3, ptr)(obj)
        size = method(obj, 4, ct.c_uint)(obj)
        return ct.string_at(address, size)

    compiler, errors, binary, compile_errors = ptr(), ptr(), ptr(), ptr()
    try:
        hr = create(source, len(source), None, None, 0, ct.byref(compiler), ct.byref(errors))
        if hr < 0:
            raise RuntimeError(data(errors).decode('utf-8', errors='replace') if errors else f'D3DX compiler: {hr:#x}')
        hr = method(compiler, 59, ct.c_long, ct.c_uint, ct.POINTER(ptr), ct.POINTER(ptr))(
            compiler, 0, ct.byref(binary), ct.byref(compile_errors))
        if hr < 0 or not binary:
            raise RuntimeError(data(compile_errors).decode('utf-8', errors='replace') if compile_errors else f'CompileEffect: {hr:#x}')
        return data(binary)
    finally:
        for obj in (compile_errors, binary, errors, compiler):
            if obj:
                method(obj, 2, ct.c_ulong)(obj)
