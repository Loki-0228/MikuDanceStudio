#include "../src/exports/effect_preview_shadow_source.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>
using namespace mikudancestudio::effect_preview_shadow;
int main(int argc, char** argv) {
    for (const unsigned original : {512u, 1024u, 2048u, 4096u, 8192u}) {
        assert(PreviewShadowSize(original, 1) == original);
        assert(PreviewShadowSize(original, 2) == original);
        assert(PreviewShadowSize(original, 0) == original);
        assert(PreviewShadowSize(original, .25) == (original / 4 < 512 ? 512 : original / 4));
        std::string source = "// Cascade shadow map for ray\nfloat ShadingShadow() {\n"
            "coord.xy += 0.5 / float(" + std::to_string(original) + ");\n"
            "float radius = 2.0 / " + std::to_string(original) + ";\n}";
        assert(AdaptRaySource(source));
        assert(source.find("OriginalSize = " + std::to_string(original)) != std::string::npos);
        assert(source.find("coord.xy += 0.5 / MMSPreviewShadowSize;") != std::string::npos);
        assert(source.find("float radius = 2.0 / " + std::to_string(original)) != std::string::npos);
        const auto adapted = source;
        assert(!AdaptRaySource(source) && source == adapted);
    }
    for (std::string source : {"coord.xy += 0.5 / float(2048);", "particle / LUT 1024",
        "Cascade shadow map for ray float ShadingShadow() { coord.xy += 0.5 / float(1234); }",
        "Cascade shadow map for ray float ShadingShadow() { coord.xy += 0.5 / float(2048); coord.xy += 0.5 / float(2048); }"}) {
        const auto original = source;
        assert(!AdaptRaySource(source) && source == original);
    }
    assert(PreviewShadowSize(256, .25) == 256);
    if (argc > 1) {
        std::ifstream input(argv[1], std::ios::binary);
        assert(input);
        std::string source{std::istreambuf_iterator<char>(input), {}};
        assert(AdaptRaySource(source));
        if (argc > 2) std::ofstream(argv[2], std::ios::binary) << source;
    }
    std::cout << "PASS: shadow source adaptation, fallback, original restore and 512 floor\n";
}
