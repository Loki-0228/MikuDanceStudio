#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace mikudancestudio {
// Model vertices and accessory world matrices are in scene coordinates. Build
// the directional-light projection there, independently of the orbit rotation.
// The range remains user controlled; both modes centre it on the view focus.
inline std::array<float, 16> NativeShadowProjection(
    const float focus[3], const float direction[3], float range, int mode) {
    float z[3] = {direction[0], direction[1], direction[2]};
    float length = std::sqrt(z[0]*z[0] + z[1]*z[1] + z[2]*z[2]);
    if (!std::isfinite(length) || length < 1e-6f) {
        z[0] = 0; z[1] = -1; z[2] = 0; length = 1;
    }
    for (float& value : z) value /= length;
    const bool pole = std::abs(z[1]) > .95f;
    float x[3] = {pole ? -z[1] : z[2], pole ? z[0] : 0.f, pole ? 0.f : -z[0]};
    length = std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
    for (float& value : x) value /= length;
    const float y[3] = {z[1]*x[2]-z[2]*x[1], z[2]*x[0]-z[0]*x[2], z[0]*x[1]-z[1]*x[0]};
    const float* axes[] = {x, y, z};
    const float base = std::isfinite(range) ? (std::max)(range, .00001f) : .01784f;
    const float scale[] = {base * (mode == 1 ? 2.f : 3.f),
                           base * (mode == 1 ? 2.f : 3.f), base * .15f};
    std::array<float, 16> matrix{};
    matrix[15] = 1;
    for (int axis = 0; axis < 3; ++axis) {
        for (int row = 0; row < 3; ++row) matrix[row*4+axis] = axes[axis][row]*scale[axis];
        matrix[12+axis] = -(focus[0]*axes[axis][0] + focus[1]*axes[axis][1] +
                            focus[2]*axes[axis][2])*scale[axis] + (axis == 2 ? .5f : 0.f);
    }
    return matrix;
}
}
