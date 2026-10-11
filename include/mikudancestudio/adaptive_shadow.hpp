#pragma once
#include <algorithm>
#include <cmath>

namespace mikudancestudio {
class MMDApp;
void LoadShadowResolutionPreference(MMDApp* app);
void SaveShadowResolutionPreference(MMDApp* app);
void UpdateAdaptiveShadowMap(MMDApp* app, const float frame[16]);
int ShadowSliderPosition(MMDApp* app);
void SetShadowSliderPosition(MMDApp* app, int position);
inline float DynamicShadowMultiplier(int position) {
    // Match the fixed range control: moving left increases shadow precision.
    return std::exp2((5000 - (std::clamp)(position, 0, 9999)) / 2500.f);
}
// Choose power-of-two maps from the projected density of visible geometry.
// Different grow/shrink thresholds prevent reallocating around a boundary.
inline unsigned AdaptiveShadowSize(float required, unsigned current, unsigned limit) {
    limit = (std::max)(512u, limit);
    if (!std::isfinite(required) || required <= 0) return limit;
    unsigned size = (std::min)(limit, (std::max)(512u, current));
    while (size < limit && required > size * 1.1f) size *= 2;
    while (size > 512 && required < size * 0.4f) size /= 2;
    return (std::min)(size, limit);
}
}
