#pragma once

#include <cstdint>
#include <string>

struct IDirect3DSurface9;
namespace mikudancestudio {
class MMDApp;
namespace image_sequence {
constexpr unsigned kMenuCommand = 32811;
struct Format { const wchar_t* name; const wchar_t* extension; int d3dxFormat; };
inline constexpr Format kFormats[] = {
    {L"BMP", L".bmp", 0}, {L"JPG", L".jpg", 1},
    {L"PNG", L".png", 3}, {L"DDS", L".dds", 4},
    {L"DIB", L".dib", 6}, {L"PFM", L".pfm", 8}, {L"HDR", L".hdr", 7},
};
struct Options {
    std::wstring directory;
    std::wstring prefix = L"frame_";
    int format = 2;
    int width = 0, height = 0;
    int start = 0, end = 0;
    float fps = 30;
    bool closeAfterExport = false;
};
const wchar_t* Text(MMDApp*, const wchar_t* zh, const wchar_t* en, const wchar_t* ja);
std::uint32_t DefaultEndFrame(MMDApp*);
std::uint64_t FrameCount(const Options&);
std::wstring FramePath(const Options&, std::uint64_t number);
bool Validate(MMDApp*, const Options&, std::wstring& error);
// Prepare checks the complete output range for collisions before rendering.
bool Prepare(MMDApp*, const Options&, std::wstring& error);
bool Active(MMDApp*);
bool WriteFrame(MMDApp*, IDirect3DSurface9*, std::wstring& error);
bool Finish(MMDApp*, bool reachedRequestedEnd, std::wstring& error);
void Abort(MMDApp*);
void RefreshMenu(MMDApp*);
bool HandleCommand(MMDApp*, unsigned command);
} // namespace image_sequence
} // namespace mikudancestudio
