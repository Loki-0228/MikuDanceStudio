#pragma once
#include <Windows.h>
#include <string>

namespace mikudancestudio {
class MMDApp;
struct WaveAudioContext;
namespace media {
bool IsAudioPath(const wchar_t* path);
bool IsVideoPath(const wchar_t* path);
bool IsModernVideoPath(const wchar_t* path);
// The source path survives project save/reload; only playback uses the temporary PCM file.
bool PrepareAudio(WaveAudioContext*, const wchar_t* source, std::wstring& playable, std::wstring& error);
const wchar_t* AudioSource(WaveAudioContext*);
void ReleaseAudio(WaveAudioContext*);
bool OpenVideo(MMDApp*, const wchar_t* path, int& width, int& height, double& duration, std::wstring& error, int maxDimension = 0);
bool HasVideo(MMDApp*);
// Bottom-up RGB32 DIB, valid until the next read or close of this reader.
BITMAPINFOHEADER* VideoFrame(MMDApp*, double seconds, std::wstring& error);
// Changes only when a different decoded frame is published, including reloads.
unsigned long long VideoFrameRevision(MMDApp*);
void ReleaseVideo(MMDApp*);
}
}
