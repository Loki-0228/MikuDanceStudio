#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "mikudancestudio/effect_api.hpp"
#include "mikudancestudio/text_encoding.hpp"
#include <cstring>
#include <string>

namespace mikudancestudio {
namespace {

// MME keeps EMM paths as bytes and calls ANSI Win32 APIs. A file saved on
// Japanese Windows can therefore point at an existing, different Unicode
// spelling on Chinese Windows. Prefer the current ACP and only fall back to
// another encoding when that exact decoded file really exists.
bool ExistingPath(const char* bytes, std::wstring& path, UINT& codePage) {
    if (!bytes || !*bytes) return false;
    bool nonAscii = false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes); *p; ++p)
        nonAscii |= *p >= 0x80;
    if (!nonAscii || (!std::strchr(bytes, '\\') && !std::strchr(bytes, '/')))
        return false;
    for (UINT cp : {GetACP(), 932u, 936u, 950u, 949u, 65001u}) {
        if (!text_encoding::Decode(bytes, cp, path)) continue;
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            codePage = cp;
            return true;
        }
    }
    return false;
}

bool MissingPath(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
           error == ERROR_INVALID_NAME || error == ERROR_NO_UNICODE_TRANSLATION;
}

DWORD WINAPI EffectFileAttributes(LPCSTR filename) {
    const DWORD attributes = GetFileAttributesA(filename);
    if (attributes != INVALID_FILE_ATTRIBUTES) return attributes;
    const DWORD error = GetLastError();
    std::wstring path;
    UINT cp = 0;
    if (MissingPath(error) && ExistingPath(filename, path, cp))
        return GetFileAttributesW(path.c_str());
    SetLastError(error);
    return attributes;
}

HANDLE WINAPI OpenEffectFile(LPCSTR filename, DWORD access, DWORD share,
                            LPSECURITY_ATTRIBUTES security, DWORD creation,
                            DWORD attributes, HANDLE templateFile) {
    HANDLE file = CreateFileA(filename, access, share, security, creation,
                              attributes, templateFile);
    if (file != INVALID_HANDLE_VALUE) return file;
    const DWORD error = GetLastError();
    std::wstring path;
    UINT cp = 0;
    // A compatibility lookup must never redirect writes or create a new file.
    if (creation == OPEN_EXISTING && (access == GENERIC_READ || access == 0) &&
        MissingPath(error) && ExistingPath(filename, path, cp))
        return CreateFileW(path.c_str(), access, share, security, creation,
                           attributes, templateFile);
    SetLastError(error);
    return file;
}

int WINAPI DecodeEffectPath(UINT codePage, DWORD flags, LPCCH bytes, int byteCount,
                           LPWSTR output, int capacity) {
    if ((codePage == CP_ACP || codePage == CP_THREAD_ACP || codePage == GetACP()) &&
        bytes && (byteCount == -1 || byteCount > 0) &&
        (flags == 0 || flags == MB_ERR_INVALID_CHARS)) {
        std::string bounded = byteCount == -1 ? std::string(bytes)
                                              : std::string(bytes, byteCount);
        const bool terminated = byteCount == -1 || bounded.back() == '\0';
        if (!bounded.empty() && bounded.back() == '\0') bounded.pop_back();
        std::wstring path;
        UINT resolvedCodePage = 0;
        if (bounded.find('\0') == std::string::npos &&
            ExistingPath(bounded.c_str(), path, resolvedCodePage) &&
            resolvedCodePage != GetACP()) {
            const int needed = static_cast<int>(path.size()) + (terminated ? 1 : 0);
            if (capacity == 0) return needed;
            if (!output || capacity < 0) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return 0;
            }
            if (capacity < needed) {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return 0;
            }
            std::memcpy(output, path.c_str(), needed * sizeof(wchar_t));
            return needed;
        }
    }
    return MultiByteToWideChar(codePage, flags, bytes, byteCount, output, capacity);
}

struct ImportReplacement { const char* name; void* original; void* replacement; };

// Restrict the compatibility calls to MMEffect's declared kernel32 imports.
// Other plugins and the host keep their normal ANSI API behavior.
void PatchImports(HMODULE module) {
    auto* base = reinterpret_cast<unsigned char*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ||
        nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) return;
    const DWORD size = nt->OptionalHeader.SizeOfImage;
    const auto& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress || imports.VirtualAddress >= size ||
        imports.Size > size - imports.VirtualAddress) return;
    const ImportReplacement replacements[] = {
        {"GetFileAttributesA", reinterpret_cast<void*>(&GetFileAttributesA),
                              reinterpret_cast<void*>(&EffectFileAttributes)},
        {"CreateFileA", reinterpret_cast<void*>(&CreateFileA),
                        reinterpret_cast<void*>(&OpenEffectFile)},
        {"MultiByteToWideChar", reinterpret_cast<void*>(&MultiByteToWideChar),
                                reinterpret_cast<void*>(&DecodeEffectPath)},
    };
    const auto* descriptors = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress);
    for (std::size_t i = 0; i < imports.Size / sizeof(*descriptors); ++i) {
        const auto& descriptor = descriptors[i];
        if (!descriptor.Name) break;
        if (descriptor.Name >= size ||
            strnlen_s(reinterpret_cast<const char*>(base + descriptor.Name), size - descriptor.Name) == size - descriptor.Name ||
            _stricmp(reinterpret_cast<const char*>(base + descriptor.Name), "kernel32.dll") != 0 ||
            !descriptor.OriginalFirstThunk || descriptor.OriginalFirstThunk >= size ||
            !descriptor.FirstThunk || descriptor.FirstThunk >= size) continue;
        const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + descriptor.OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor.FirstThunk);
        const auto nameCount = (size - descriptor.OriginalFirstThunk) / sizeof(*names);
        const auto slotCount = (size - descriptor.FirstThunk) / sizeof(*slots);
        for (std::size_t j = 0; j < nameCount && j < slotCount && names[j].u1.AddressOfData; ++j) {
            if (IMAGE_SNAP_BY_ORDINAL(names[j].u1.Ordinal)) continue;
            const auto rva = names[j].u1.AddressOfData;
            if (rva >= size || size - rva <= sizeof(WORD)) continue;
            const char* name = reinterpret_cast<const char*>(base + rva + sizeof(WORD));
            if (strnlen_s(name, size - rva - sizeof(WORD)) == size - rva - sizeof(WORD)) continue;
            for (const auto& entry : replacements) {
                if (std::strcmp(name, entry.name) != 0 ||
                    slots[j].u1.Function != reinterpret_cast<ULONG_PTR>(entry.original)) continue;
                DWORD protection = 0;
                auto* slot = &slots[j].u1.Function;
                if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) continue;
                InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot),
                                                  entry.replacement, entry.original);
                DWORD ignored = 0;
                VirtualProtect(slot, sizeof(void*), protection, &ignored);
            }
        }
    }
}
}  // namespace

void InstallEffectPathCompatibility() {
    const HMODULE effect = GetModuleHandleW(L"MMEffect.dll");
    if (effect) PatchImports(effect);
}
}  // namespace mikudancestudio
