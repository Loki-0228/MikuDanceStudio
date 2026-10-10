#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "mikudancestudio/text_encoding.hpp"
#include "mikudancestudio/model.hpp"
#include <cstring>
#include <cwchar>
#include <unordered_map>

namespace mikudancestudio::text_encoding {
namespace { std::unordered_map<const mdl::ModelRecord*,MaterialNames> materialNames; }
void SetMaterialNames(const mdl::ModelRecord& model,MaterialNames names){materialNames[&model]=std::move(names);}
void ForgetMaterialNames(const mdl::ModelRecord& model){materialNames.erase(&model);}
std::wstring MaterialName(const mdl::ModelRecord& model,std::size_t index,bool english){
    const auto found=materialNames.find(&model);
    if(found==materialNames.end()||index>=found->second.size())return {};
    const auto& names=found->second[index];return names[english?1:0].empty()?names[english?0:1]:names[english?1:0];
}
bool HasUtf8Bom(const char* text) {
    return text && std::strncmp(text, "\xEF\xBB\xBF", 3) == 0;
}

bool Decode(const char* bytes, UINT codePage, std::wstring& result) {
    result.clear();
    if (!bytes) return false;
    const int count = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS,
                                         bytes, -1, nullptr, 0);
    if (!count) return false;
    result.resize(count);
    if (!MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, bytes, -1,
                             result.data(), count)) {
        result.clear();
        return false;
    }
    result.resize(count - 1);
    return true;
}

bool EncodeExact(const wchar_t* text, UINT codePage, std::string& result) {
    result.clear();
    if (!text) text = L"";
    const UINT cp = codePage == CP_ACP ? GetACP() : codePage;
    const DWORD flags = cp == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    BOOL replaced = FALSE;
    BOOL* replacement = cp == CP_UTF8 ? nullptr : &replaced;
    int count = WideCharToMultiByte(cp, flags, text, -1, nullptr, 0, nullptr, replacement);
    if (!count || replaced) return false;
    result.resize(count);
    if (!WideCharToMultiByte(cp, flags, text, -1, result.data(), count, nullptr, replacement)
        || replaced) {
        result.clear();
        return false;
    }
    result.resize(count - 1);
    // Reject even reversible-looking best-fit substitutions (e.g. yen/backslash).
    std::wstring decoded;
    if (!Decode(result.c_str(), cp, decoded) || decoded != text) {
        result.clear();
        return false;
    }
    return true;
}

namespace {
bool Store(char* dst, std::size_t capacity, const std::string& value) {
    if (!dst || !capacity) return false;
    std::memset(dst, 0, capacity);
    if (value.size() >= capacity) return false;
    std::memcpy(dst, value.data(), value.size());
    return true;
}
bool StoreUtf8(char* dst, std::size_t capacity, const wchar_t* text) {
    std::string encoded;
    if (!EncodeExact(text, CP_UTF8, encoded)) return Store(dst, capacity, ""), false;
    return Store(dst, capacity, "\xEF\xBB\xBF" + encoded);
}
}

bool EncodePath(char* dst, std::size_t capacity, const wchar_t* path) {
    std::string encoded;
    for (UINT cp : {932u, GetACP(), 936u, 950u, 949u})
        if (EncodeExact(path, cp, encoded) && encoded.size() < capacity)
            return Store(dst, capacity, encoded);
    return StoreUtf8(dst, capacity, path);
}

bool EncodeDisplay(char* dst, std::size_t capacity, const wchar_t* text) {
    std::string encoded;
    if (EncodeExact(text, CP_ACP, encoded) && encoded.size() < capacity)
        return Store(dst, capacity, encoded);
    return StoreUtf8(dst, capacity, text);
}

std::wstring Display(const char* text) {
    std::wstring result;
    if (HasUtf8Bom(text)) Decode(text + 3, CP_UTF8, result);
    else Decode(text, CP_ACP, result);
    return result;
}

std::wstring LegacyFilename(const char* text, const wchar_t* resolvedPath) {
    if (HasUtf8Bom(text)) return Display(text);
    const wchar_t* tail = std::wcsrchr(resolvedPath, L'\\');
    tail = tail ? tail + 1 : resolvedPath;
    std::wstring decoded;
    // Existence-resolved Unicode path disambiguates untagged Chinese/Japanese
    // filenames. Preserve custom labels rather than replacing them with a path.
    for (UINT cp : {GetACP(), 932u, 936u, 950u, 949u, 65001u})
        if (Decode(text, cp, decoded) && _wcsicmp(decoded.c_str(), tail) == 0)
            return decoded;
    return Display(text);
}

std::wstring ModelLabel(const mdl::ModelRecord& model, bool english) {
    const wchar_t* filename = model.path;
    for (const wchar_t* p = filename; *p; ++p)
        if (*p == L'\\' || *p == L'/') filename = p + 1;
    if (*filename) return filename;
    return ModelName(model, english);
}

std::wstring ModelName(const mdl::ModelRecord& model, bool english) {
    const auto* wide = model.pmxTextBuffers[english ? 1 : 0];
    if (wide && *wide) return wide;
    wide = model.pmxTextBuffers[english ? 0 : 1];
    if (wide && *wide) return wide;
    const char* narrow = english ? model.nameEn : model.name;
    if (!*narrow) narrow = english ? model.name : model.nameEn;
    std::wstring result;
    if (HasUtf8Bom(narrow)) return Display(narrow);
    // PMD model names are Shift-JIS; unlike untagged paths there is no file
    // existence check that can disambiguate arbitrary customized model names.
    if (Decode(narrow, 932, result)) return result;
    return Display(narrow);
}

std::wstring MorphName(const mdl::MorphRecord& morph, bool english) {
    const wchar_t* wide = english ? morph.enText : morph.jpText;
    if (wide && *wide) return wide;
    wide = english ? morph.jpText : morph.enText;
    if (wide && *wide) return wide;
    const char* narrow = english ? morph.nameEn : morph.name;
    if (!*narrow) narrow = english ? morph.name : morph.nameEn;
    // PMD/VMD names are CP932, independent of the Windows/UI language.
    std::wstring result;
    const std::string bounded(narrow, strnlen_s(narrow, sizeof(morph.name)));
    Decode(bounded.c_str(), 932, result);
    return result;
}

std::wstring BoneName(const mdl::BoneRecord& bone, bool english) {
    const wchar_t* wide = english ? bone.enText : bone.jpText;
    if (wide && *wide) return wide;
    wide = english ? bone.jpText : bone.enText;
    if (wide && *wide) return wide;
    const char* narrow = english ? bone.nameEn : bone.name;
    if (!*narrow) narrow = english ? bone.name : bone.nameEn;
    std::wstring result;
    const std::string bounded(narrow, strnlen_s(narrow, sizeof(bone.name)));
    Decode(bounded.c_str(), 932, result);
    return result;
}

std::wstring DisplayGroupName(const mdl::DisplayGroup& group, bool english) {
    const char* narrow = english ? group.nameEn : group.name;
    if (!*narrow) narrow = english ? group.name : group.nameEn;
    // These legacy model fields are CP932, unlike accessory display labels.
    const std::string bounded(narrow, strnlen_s(narrow, sizeof(group.name)));
    std::wstring result;
    Decode(bounded.c_str(), 932, result);
    return result;
}

int MessageBoxJp(void* owner, const char* body, const char* caption,
                 unsigned int flags) {
    std::wstring wideBody;
    std::wstring wideCaption;
    // The JP literals are byte-exact Shift-JIS; ASCII (the EN branch) decodes
    // identically under CP932, so both branches can share this path.
    if (!Decode(body, 932, wideBody)) wideBody = Display(body);
    if (!Decode(caption, 932, wideCaption)) wideCaption = Display(caption);
    return MessageBoxW(static_cast<HWND>(owner), wideBody.c_str(),
                       wideCaption.c_str(), flags);
}
}
