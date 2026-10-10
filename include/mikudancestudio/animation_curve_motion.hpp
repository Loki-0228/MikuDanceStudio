#pragma once
#include "mikudancestudio/animation_curve.hpp"
namespace mikudancestudio::curve_motion {
enum class Kind : uint32_t { Camera, Light, Shadow, Gravity, Accessory, Effect, Morph };
// One motion source.  Track ids follow the curve editor's channel ids:
//   Camera    x, y, z, rx, ry, rz (degrees), distance, fov, perspective
//   Light     x, y, z (direction), r, g, b
//   Shadow    mode, distance
//   Gravity   strength, x, y, z, noise, noiseEnabled
//   Accessory x, y, z, rx, ry, rz (degrees), scale, opacity, visible, shadow,
//             parentModel, parentBone
//   Effect / Morph  arbitrary parameter ids, stored verbatim in the tail
struct Motion { Kind kind=Kind::Accessory; std::vector<animation_curve::NamedTrack> tracks; };
// All six standard VMD sections precede the optional, checksummed tail.
// Exports bake a per-frame proxy so readers unaware of the curve extension
// still reproduce the motion at integer frames; the tail holds the exact curve
// data and the real track ids.  Decode falls back to reading the standard
// proxy when no tail is present: accessory samples its single bone track
// through the MMD interpolation channels, camera, light and self-shadow keep
// their native sections, and proxy names that do not fit the 15-byte field are
// kept as their checksum placeholder instead of being guessed back.
bool Encode(const Motion&,std::vector<uint8_t>&,std::string&error);
bool Decode(const uint8_t*,size_t,Kind requested,Motion&,std::string&error);
bool Save(const wchar_t*path,const Motion&,std::string&error);
bool Load(const wchar_t*path,Kind requested,Motion&,std::string&error);
}
