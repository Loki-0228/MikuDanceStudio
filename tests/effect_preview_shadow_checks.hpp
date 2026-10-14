void CheckEffectPreviewShadowTargets(int percent) {
    if (std::getenv("MMS_NO_MME_PREVIEW_SHADOW")) return; // Unmodified export control.
    const auto targets = effect_preview_shadow::InspectTargets();
    Check(targets.size() >= 4, "real MME recognized shadow resources");
    for (const auto& target : targets) {
        // The real Stage fixtures use 1024 (SoftShadow) and 2048 (ray).
        const unsigned expected = percent == 100 ? target.original : 512;
        std::printf("SCALE_MME_SHADOW percent=%d kind=%s original=%u actual=%ux%u\n",
            percent, target.soft ? "soft" : "ray", target.original, target.width, target.height);
        Check(target.width == expected && target.height == expected,
            "MME color and depth resources follow preview and restore original dimensions");
    }
}

void CheckEffectPreviewShadowExport(MMDApp* app, const std::filesystem::path& directory) {
    std::wstring error;
    Check(viewport_render_scale::Configure(app, false, 25, error), "reduced preview before shadow export");
    const auto frame = [&] {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        app->DeltaTime() = 0; app->state.messageSeen = 10; FrameDriver(app);
    };
    for (int i = 0; i < 8; ++i) frame();
    CheckEffectPreviewShadowTargets(25);
    Check(PostDeviceReset(app), "device reset while reduced shadow preview remains selected");
    for (int i = 0; i < 8; ++i) frame();
    CheckEffectPreviewShadowTargets(25);
    const auto avi = directory / L"shadow-restore.avi";
    wcscpy_s(app->AviOutputPath(), 256, avi.c_str());
    app->Recorder()->selectedCodec = -1;
    app->AviOutputWidth() = 640; app->AviOutputHeight() = 480;
    app->AviRecordStartFrame() = 0; app->AviRecordEndFrame() = 2;
    app->AviRecordFps() = 30; app->AviIncludeWave() = 0; app->AviCloseAfterExport() = 0;
    StartAviRecordWindow(app);
    Check(app->RecordingWindow() != nullptr, "start real AVI from reduced MME preview");
    unsigned exported = 0;
    for (unsigned i = 0; i < 120 && app->RecordingWindow(); ++i) {
        frame();
        if (app->RecordingWindow()) { CheckEffectPreviewShadowTargets(100); ++exported; }
    }
    Check(exported > 0 && !app->RecordingWindow(), "AVI finishes with full shadow precision");
    for (int i = 0; i < 8; ++i) frame();
    CheckEffectPreviewShadowTargets(25);
    Check(viewport_render_scale::Configure(app, false, 100, error), "restore full preview after export");
    for (int i = 0; i < 8; ++i) frame();
    CheckEffectPreviewShadowTargets(100);
}
