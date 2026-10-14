#pragma once
#include <vector>
namespace mikudancestudio {
class MMDApp;
namespace effect_preview_shadow {
// Known native MME only; unknown binaries/effects retain their own dimensions.
void Install();
// Call before MME's BeginScene, including unscaled and export frames.
void PrepareFrame(MMDApp* app);
struct TargetInfo { unsigned original, width, height; bool soft; };
// Read-only diagnostics; never called by the normal per-frame path.
std::vector<TargetInfo> InspectTargets();
}
}
