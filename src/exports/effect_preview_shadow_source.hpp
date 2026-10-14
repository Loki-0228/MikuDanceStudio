#pragma once
#include <regex>
#include <string>

namespace mikudancestudio::effect_preview_shadow {
// Work on D3DX-preprocessed text, never on users' FX files. Keep the cascade
// projection and normalized blur radius identical on both caster and receiver.
// Only the half-texel sampling offset needs the actual reduced texture size.
inline bool AdaptRaySource(std::string& source) {
    if (source.find("MMSPreviewShadowSize") != std::string::npos ||
        source.find("Cascade shadow map for ray") == std::string::npos ||
        source.find("float ShadingShadow") == std::string::npos) return false;
    static const std::regex offset(R"(coord\s*\.\s*xy\s*\+=\s*0\.5\s*/\s*float\s*\(\s*(512|1024|2048|4096|8192)\s*\)\s*;)");
    std::smatch match;
    if (!std::regex_search(source, match, offset)) return false;
    const std::string original = match[1];
    if (std::regex_search(match.suffix().str(), offset)) return false;
    source.replace(size_t(match.position()), size_t(match.length()),
        "coord.xy += 0.5 / MMSPreviewShadowSize;");
    // An immutable annotation retains the FX author's original setting.
    source.insert(0, "float MMSPreviewShadowSize < int OriginalSize = " + original +
        "; > = " + original + ";\n");
    return true;
}
inline unsigned PreviewShadowSize(unsigned original, double factor) {
    if (!(factor > 0 && factor < 1) || original < 512) return original;
    const double required = (original * factor < 512) ? 512 : original * factor;
    unsigned result = original;
    while (result / 2 >= required) result /= 2;
    return result;
}
}
