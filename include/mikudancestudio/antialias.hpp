#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <memory>
#include <string>

namespace mikudancestudio {
class MMDApp;
namespace antialias {
enum class Algorithm : unsigned { MmdOriginal, Msaa2, Msaa4, Msaa8, Fxaa, Taa, Count };
inline constexpr unsigned kFirstCommand = 34900;
struct Settings {
    Algorithm algorithm = Algorithm::MmdOriginal;
    bool enabled = true;
    bool initialized = false;
    unsigned originalSamples = 0, originalQuality = 0;
};
void Initialize(MMDApp* app);
void RefreshMenu(MMDApp* app);
bool Supported(MMDApp* app, Algorithm algorithm);
bool Select(MMDApp* app, Algorithm algorithm, std::wstring& error);
bool Toggle(MMDApp* app, std::wstring& error);
void ApplyRenderState(MMDApp* app);
void PrepareFrame(MMDApp* app);
// True while an enabled post-process algorithm is preparing its first frame.
bool Initializing(MMDApp* app);
unsigned AccumulatedSamples(MMDApp* app);
void Release(MMDApp* app);
bool HandleMessage(MMDApp* app, UINT message, WPARAM wp, LPARAM lp);
LRESULT CALLBACK MenuFilter(int code, WPARAM wp, LPARAM lp);
// Owns a single-sample scene for post-process AA. MME sees one ordinary frame;
// the final shader runs on the native device after MME's EndScene callbacks.
class FrameScope {
public:
    explicit FrameScope(MMDApp* app);
    ~FrameScope();
    bool Ready() const;
    bool Complete();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
}
