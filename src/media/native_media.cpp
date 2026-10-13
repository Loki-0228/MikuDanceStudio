#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "mikudancestudio/native_media.hpp"
#include "mikudancestudio/wave_audio_context.hpp"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace mikudancestudio::media {
using Microsoft::WRL::ComPtr;
namespace {
struct Failure { HRESULT hr; const wchar_t* operation; };
void Check(HRESULT hr, const wchar_t* operation) { if (FAILED(hr)) throw Failure{hr, operation}; }
std::wstring Describe(const Failure& failure) {
    wchar_t code[32]{}; swprintf_s(code, L" (0x%08lX)", failure.hr);
    return std::wstring(failure.operation) + code + L"\nWindows 无法解码该文件。请检查文件及系统媒体组件。";
}
struct Runtime {
    bool com = false, mf = false;
    Runtime() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        com = SUCCEEDED(hr);
        if (hr != RPC_E_CHANGED_MODE) Check(hr, L"COM initialization");
        const HRESULT startup = MFStartup(MF_VERSION);
        if (FAILED(startup)) { if (com) CoUninitialize(); com = false; Check(startup, L"Media Foundation initialization"); }
        mf = true;
    }
    ~Runtime() { if (mf) MFShutdown(); if (com) CoUninitialize(); }
};
bool Extension(const wchar_t* path, const wchar_t* extension) {
    if (!path) return false;
    const wchar_t* dot = std::wcsrchr(path, L'.');
    return dot && _wcsicmp(dot, extension) == 0;
}
struct DecodedAudio {
    std::wstring source, temporary;
    ~DecodedAudio() { if (!temporary.empty()) DeleteFileW(temporary.c_str()); }
};
std::unordered_map<WaveAudioContext*, std::unique_ptr<DecodedAudio>> audioFiles;

struct Video {
    Runtime runtime;
    ComPtr<IMFSourceReader> reader;
    std::vector<unsigned char> dib;
    UINT width = 0, height = 0;
    LONG stride = 0;
    LONGLONG time = -1, end = -1, frameDuration = 333333;
    LONGLONG origin = 0;
    bool originKnown = false;
    double duration = 0;
    bool eos = false;
    unsigned long long revision = 0;

    void Copy(IMFSample* sample) {
        ComPtr<IMFMediaBuffer> buffer;
        Check(sample->ConvertToContiguousBuffer(&buffer), L"Video frame buffer");
        ComPtr<IMF2DBuffer> twoD;
        BYTE* data = nullptr;
        LONG pitch = stride;
        DWORD bytes = 0;
        const bool use2D = SUCCEEDED(buffer.As(&twoD));
        if (use2D) Check(twoD->Lock2D(&data, &pitch), L"Lock video frame");
        else {
            Check(buffer->Lock(&data, nullptr, &bytes), L"Lock video frame");
            if (bytes < static_cast<std::uint64_t>(std::abs(static_cast<LONGLONG>(pitch))) * height) {
                buffer->Unlock(); throw Failure{E_INVALIDARG, L"Truncated video frame"};
            }
            if (pitch < 0) data += static_cast<size_t>(-static_cast<LONGLONG>(pitch)) * (height - 1);
        }
        if (std::abs(static_cast<LONGLONG>(pitch)) < static_cast<LONGLONG>(width) * 4) {
            if (use2D) twoD->Unlock2D(); else buffer->Unlock();
            throw Failure{E_INVALIDARG, L"Invalid video row stride"};
        }
        auto* pixels = dib.data() + sizeof(BITMAPINFOHEADER);
        for (UINT y = 0; y < height; ++y)
            std::memcpy(pixels + static_cast<size_t>(height - 1 - y) * width * 4,
                        data + static_cast<ptrdiff_t>(y) * pitch, width * 4);
        if (use2D) twoD->Unlock2D(); else buffer->Unlock();
    }
};
std::unordered_map<MMDApp*, std::unique_ptr<Video>> videos;
unsigned long long nextVideoRevision = 0;
}

bool IsAudioPath(const wchar_t* path) {
    for (const auto* ext : {L".wav", L".mp3", L".aac", L".adts", L".m4a", L".wma"})
        if (Extension(path, ext)) return true;
    return false;
}
bool IsModernVideoPath(const wchar_t* path) { return Extension(path, L".mp4") || Extension(path, L".mov"); }
bool IsVideoPath(const wchar_t* path) { return Extension(path, L".avi") || IsModernVideoPath(path); }
void ReleaseAudio(WaveAudioContext* audio) { audioFiles.erase(audio); }
const wchar_t* AudioSource(WaveAudioContext* audio) {
    const auto it = audioFiles.find(audio);
    return it == audioFiles.end() ? audio->path : it->second->source.c_str();
}
bool PrepareAudio(WaveAudioContext* audio, const wchar_t* source, std::wstring& playable, std::wstring& error) {
    playable = source; error.clear();
    if (Extension(source, L".wav")) return true;
    try {
        Runtime runtime;
        ComPtr<IMFSourceReader> reader;
        Check(MFCreateSourceReaderFromURL(source, nullptr, &reader), L"Open audio");
        Check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE), L"Select audio");
        Check(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE), L"Select audio");
        ComPtr<IMFMediaType> type;
        Check(MFCreateMediaType(&type), L"Audio media type");
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        Check(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get()), L"Decode PCM audio");
        type.Reset();
        Check(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type), L"PCM media type");
        UINT32 rate = 0, channels = 0, bits = 0;
        Check(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate), L"Audio sample rate");
        Check(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels), L"Audio channels");
        Check(type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits), L"Audio bit depth");
        if (!rate || channels < 1 || channels > 2 || bits != 16)
            throw Failure{MF_E_INVALIDMEDIATYPE, L"Audio must decode to mono or stereo PCM16"};
        wchar_t folder[MAX_PATH]{}, temporary[MAX_PATH]{};
        if (!GetTempPathW(MAX_PATH, folder) || !GetTempFileNameW(folder, L"MMS", 0, temporary))
            throw Failure{HRESULT_FROM_WIN32(GetLastError()), L"Create decoded audio cache"};
        auto decoded = std::make_unique<DecodedAudio>();
        decoded->temporary = temporary; decoded->source = source;
        FILE* raw = nullptr;
        if (_wfopen_s(&raw, temporary, L"w+b") || !raw) throw Failure{E_ACCESSDENIED, L"Open audio cache"};
        std::unique_ptr<FILE, decltype(&std::fclose)> file(raw, &std::fclose);
        unsigned char header[44]{};
        std::memcpy(header, "RIFF", 4); std::memcpy(header + 8, "WAVEfmt ", 8); std::memcpy(header + 36, "data", 4);
        const auto put16 = [&](unsigned offset, std::uint16_t value) { std::memcpy(header + offset, &value, 2); };
        const auto put32 = [&](unsigned offset, std::uint32_t value) { std::memcpy(header + offset, &value, 4); };
        put32(16, 16); put16(20, 1); put16(22, static_cast<std::uint16_t>(channels));
        put32(24, rate); put32(28, rate * channels * 2); put16(32, static_cast<std::uint16_t>(channels * 2)); put16(34, 16);
        if (std::fwrite(header, 1, 44, raw) != 44) throw Failure{E_FAIL, L"Write WAV header"};
        std::uint64_t total = 0;
        for (;;) {
            DWORD flags = 0; ComPtr<IMFSample> sample;
            Check(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample), L"Read audio");
            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) throw Failure{MF_E_INVALIDMEDIATYPE, L"Audio format changed"};
            if (sample) {
                ComPtr<IMFMediaBuffer> buffer;
                Check(sample->ConvertToContiguousBuffer(&buffer), L"PCM buffer");
                BYTE* data = nullptr; DWORD count = 0;
                Check(buffer->Lock(&data, nullptr, &count), L"Lock PCM buffer");
                const bool fits = total + count <= INT32_MAX - 44;
                const bool written = fits && std::fwrite(data, 1, count, raw) == count;
                buffer->Unlock();
                if (!written) throw Failure{E_FAIL, fits ? L"Write audio cache" : L"Decoded audio exceeds 2 GiB"};
                total += count;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        }
        if (!total || total % (channels * 2)) throw Failure{E_FAIL, L"Empty or incomplete audio"};
        put32(4, static_cast<std::uint32_t>(36 + total)); put32(40, static_cast<std::uint32_t>(total));
        if (std::fseek(raw, 0, SEEK_SET) || std::fwrite(header, 1, 44, raw) != 44 || std::fflush(raw))
            throw Failure{E_FAIL, L"Finalize audio cache"};
        file.reset();
        playable = decoded->temporary;
        audioFiles[audio] = std::move(decoded);
        return true;
    } catch (const Failure& failure) { error = Describe(failure); }
      catch (const std::exception&) { error = L"音频解码内存不足。"; }
    return false;
}

bool HasVideo(MMDApp* app) { return videos.find(app) != videos.end(); }
unsigned long long VideoFrameRevision(MMDApp* app) {
    const auto it = videos.find(app);
    return it == videos.end() ? 0 : it->second->revision;
}
void ReleaseVideo(MMDApp* app) { videos.erase(app); }
bool OpenVideo(MMDApp* app, const wchar_t* path, int& width, int& height, double& duration, std::wstring& error, int maxDimension) {
    error.clear();
    try {
        auto video = std::make_unique<Video>();
        ComPtr<IMFAttributes> attributes;
        Check(MFCreateAttributes(&attributes, 1), L"Video reader attributes");
        attributes->SetUINT32(maxDimension > 0 ? MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING
                                              : MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        HRESULT opened = MFCreateSourceReaderFromURL(path, attributes.Get(), &video->reader);
        if (FAILED(opened) && maxDimension > 0) {
            attributes->DeleteItem(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING);
            attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            opened = MFCreateSourceReaderFromURL(path, attributes.Get(), &video->reader);
            maxDimension = 0;
        }
        Check(opened, L"Open video");
        auto* reader = video->reader.Get();
        Check(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE), L"Select video");
        Check(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE), L"Select video");
        ComPtr<IMFMediaType> type;
        Check(MFCreateMediaType(&type), L"Video media type");
        type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        ComPtr<IMFMediaType> nativeType;
        UINT32 nativeWidth = 0, nativeHeight = 0;
        if (maxDimension > 0 &&
            SUCCEEDED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeType)) &&
            SUCCEEDED(MFGetAttributeSize(nativeType.Get(), MF_MT_FRAME_SIZE, &nativeWidth, &nativeHeight)) &&
            nativeWidth && nativeHeight && std::max(nativeWidth, nativeHeight) > UINT32(maxDimension)) {
            // The background is drawn through a 1024-square texture. Resize
            // before RGB conversion instead of uploading a full 4K/8K bitmap
            // only to downsample it to the same display texture immediately.
            const double scale = double(maxDimension) / std::max(nativeWidth, nativeHeight);
            MFSetAttributeSize(type.Get(), MF_MT_FRAME_SIZE,
                std::max(2u, UINT32(nativeWidth * scale) & ~1u),
                std::max(2u, UINT32(nativeHeight * scale) & ~1u));
        }
        HRESULT format = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.Get());
        if (FAILED(format)) {
            type->DeleteItem(MF_MT_FRAME_SIZE);
            format = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type.Get());
        }
        Check(format, L"Decode RGB32 video");
        type.Reset(); Check(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type), L"RGB32 media type");
        Check(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &video->width, &video->height), L"Video dimensions");
        if (!video->width || !video->height || video->width > 16384 || video->height > 16384)
            throw Failure{E_INVALIDARG, L"Invalid video dimensions"};
        UINT32 stride = 0;
        if (SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) video->stride = static_cast<LONG>(stride);
        else Check(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1, video->width, &video->stride), L"Video row stride");
        UINT32 numerator = 0, denominator = 0;
        if (SUCCEEDED(MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &numerator, &denominator)) && numerator && denominator)
            video->frameDuration = 10000000LL * denominator / numerator;
        PROPVARIANT length{};
        if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &length)) && length.vt == VT_UI8)
            video->duration = length.uhVal.QuadPart / 10000000.0;
        PropVariantClear(&length);
        if (!std::isfinite(video->duration) || video->duration <= 0) throw Failure{E_INVALIDARG, L"Video has no finite duration"};
        video->dib.resize(sizeof(BITMAPINFOHEADER) + static_cast<size_t>(video->width) * video->height * 4);
        auto* header = reinterpret_cast<BITMAPINFOHEADER*>(video->dib.data());
        *header = {}; header->biSize = sizeof(*header); header->biWidth = video->width; header->biHeight = video->height;
        header->biPlanes = 1; header->biBitCount = 32; header->biCompression = BI_RGB;
        width = video->width; height = video->height; duration = video->duration;
        videos[app] = std::move(video);
        if (!VideoFrame(app, 0, error)) { ReleaseVideo(app); return false; }
        return true;
    } catch (const Failure& failure) { error = Describe(failure); }
      catch (const std::exception&) { error = L"视频解码内存不足。"; }
    return false;
}
BITMAPINFOHEADER* VideoFrame(MMDApp* app, double seconds, std::wstring& error) {
    const auto found = videos.find(app);
    if (found == videos.end()) return nullptr;
    auto& video = *found->second;
    try {
        if (!std::isfinite(seconds)) seconds = 0;
        const LONGLONG target = static_cast<LONGLONG>(std::clamp(seconds, 0.0, std::max(0.0, video.duration - .0000001)) * 10000000.0);
        if (video.time >= 0 && target >= video.time && (target < video.end || video.eos))
            return reinterpret_cast<BITMAPINFOHEADER*>(video.dib.data());
        if (video.time >= 0 && (target < video.time || target > video.end + 10000000)) {
            PROPVARIANT position{}; position.vt = VT_I8; position.hVal.QuadPart = target;
            Check(video.reader->SetCurrentPosition(GUID_NULL, position), L"Seek video");
            video.time = video.end = -1; video.eos = false;
        }
        ComPtr<IMFSample> selected;
        for (;;) {
            ComPtr<IMFSample> sample; DWORD flags = 0; LONGLONG time = 0;
            Check(video.reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &time, &sample), L"Read video frame");
            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) throw Failure{MF_E_INVALIDMEDIATYPE, L"Video dimensions changed"};
            if (sample) {
                // MP4 composition timestamps can start after zero (for example
                // B-frame reordering with an edit list). MMD's clip time starts
                // at the first decoded frame. Seeking uses presentation time.
                if (!video.originKnown) { video.origin = time; video.originKnown = true; }
                selected = sample;
                video.time = time - video.origin;
                LONGLONG duration = video.frameDuration;
                sample->GetSampleDuration(&duration);
                video.end = video.time + std::max<LONGLONG>(duration, 1);
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) video.eos = true;
            if ((sample && video.end > target) || video.eos) break;
        }
        if (video.time < 0) throw Failure{E_FAIL, L"Video has no frames"};
        // Seeking/catching up may decode an entire GOP. Copy only the frame
        // that will actually be displayed, not every discarded sample.
        if (selected) {
            video.Copy(selected.Get());
            video.revision = ++nextVideoRevision;
        }
        return reinterpret_cast<BITMAPINFOHEADER*>(video.dib.data());
    } catch (const Failure& failure) { error = Describe(failure); return nullptr; }
}
}
