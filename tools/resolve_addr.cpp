// resolve_addr.cpp - map a crash offset (RVA) in an MSVC PDB-backed image
// to function + source file:line using DbgHelp.
//
// usage: resolve_addr.exe <exe-or-dll-path> <offset-hex>
//   e.g. resolve_addr.exe build\Release\MikuMikuDanceE.exe 82DF5
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#include <cstdio>
#include <cstdlib>

static void PrintSym(HANDLE proc, DWORD64 address) {
    // Symbol + function name.
    char symbolBuffer[sizeof(SYMBOL_INFO) + 1024];
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 1024;
    DWORD64 displacement = 0;
    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
        ok = SymFromAddr(proc, address, &displacement, sym);
        if (!ok) Sleep(50);
    }
    if (ok) {
        printf("FUNC  %s + 0x%llX (at 0x%llX)\n", sym->Name,
               (unsigned long long)displacement,
               (unsigned long long)address);
    } else {
        printf("FUNC  <unresolved> (SymFromAddr err %lu)\n", GetLastError());
    }

    // Undecorated name.
    char und[1024];
    if (sym->Name[0] &&
        UnDecorateSymbolName(sym->Name, und, sizeof(und), UNDNAME_COMPLETE)) {
        printf("UND   %s\n", und);
    }

    // Source line.
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(line);
    DWORD lineDisp = 0;
    if (SymGetLineFromAddr64(proc, address, &lineDisp, &line)) {
        printf("LINE  %s : %lu (+0x%lX)\n", line.FileName,
               (unsigned long)line.LineNumber, lineDisp);
    } else {
        printf("LINE  <no line info> (err %lu)\n", GetLastError());
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: resolve_addr <image> <offset-hex>\n");
        return 1;
    }
    const char* imagePath = argv[1];
    unsigned long long offset = 0;
    if (sscanf_s(argv[2], "%llx", &offset) != 1) {
        printf("bad offset '%s'\n", argv[2]);
        return 1;
    }

    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
                  SYMOPT_DEBUG);

    // Extract the directory containing the image so the matching .pdb is found.
    char dir[MAX_PATH];
    DWORD dirLen = GetFullPathNameA(imagePath, MAX_PATH, dir, nullptr);
    if (dirLen == 0 || dirLen >= MAX_PATH) {
        printf("GetFullPathNameA failed\n");
        return 2;
    }
    char* slash = strrchr(dir, '\\');
    if (slash) *slash = '\0';

    if (!SymInitialize(proc, dir, FALSE)) {
        printf("SymInitialize failed %lu\n", GetLastError());
        return 2;
    }

    // Load the module at a fixed base so the RVA maps to base+offset.
    const DWORD64 base = 0x10000000;
    DWORD64 loadedBase = SymLoadModuleEx(proc, nullptr, imagePath, nullptr,
                                         base, 0, nullptr, 0);
    if (loadedBase == 0) {
        printf("SymLoadModuleEx failed %lu\n", GetLastError());
        return 2;
    }
    printf("IMAGE %s\nBASE  0x%llX (loaded 0x%llX)\n", imagePath,
           (unsigned long long)base, (unsigned long long)loadedBase);

    IMAGEHLP_MODULE64 mi;
    mi.SizeOfStruct = sizeof(mi);
    if (SymGetModuleInfo64(proc, loadedBase, &mi)) {
        const char* types[] = {"SymNone","SymCoff","SymCv","SymPdb","SymExport","SymDeferred","SymSym","SymDia","SymVirtual"};
        const char* t = (mi.SymType >= 0 && mi.SymType <= 8) ? types[mi.SymType] : "?";
        printf("MOD   %s\nSYMTYPE %s (%d)\nLOADEDIMAGE %s\n", mi.ModuleName, t, mi.SymType, mi.LoadedImageName);
    } else {
        printf("MOD   <SymGetModuleInfo64 err %lu>\n", GetLastError());
    }

    // Probe: force the deferred symbol load and confirm the PDB is usable.
    {
        char probeBuf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* probe = reinterpret_cast<SYMBOL_INFO*>(probeBuf);
        probe->SizeOfStruct = sizeof(SYMBOL_INFO);
        probe->MaxNameLen = 256;
        if (SymFromName(proc, "WinMain", probe)) {
            printf("PROBE WinMain = 0x%llX\n", (unsigned long long)probe->Address);
        } else {
            printf("PROBE WinMain <not found, err %lu>\n", GetLastError());
        }
        IMAGEHLP_MODULE64 mi2;
        mi2.SizeOfStruct = sizeof(mi2);
        if (SymGetModuleInfo64(proc, loadedBase, &mi2)) {
            const char* types[] = {"SymNone","SymCoff","SymCv","SymPdb","SymExport","SymDeferred","SymSym","SymDia","SymVirtual"};
            const char* t2 = (mi2.SymType >= 0 && mi2.SymType <= 8) ? types[mi2.SymType] : "?";
            printf("SYMTYPE2 %s (%d)\n", t2, mi2.SymType);
        }
    }
    PrintSym(proc, base + offset);

    SymCleanup(proc);
    return 0;
}
