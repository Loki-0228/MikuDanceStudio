// Included by the hidden real-MME display host.
void CheckGroundShadow(MMDApp* app) {
    auto** vtable = *reinterpret_cast<void***>(app->Renderer()->device);
    Check(MH_CreateHook(vtable[17], &ObservePresent, reinterpret_cast<void**>(&originalPresent)) == MH_OK &&
          MH_EnableHook(vtable[17]) == MH_OK, "capture real ground-shadow presentation");
    app->GroundGridEnabled() = false;
    app->FpsOverlayEnabled() = false;
    CommandDispatch(nullptr, MAKEWPARAM(272, 0));
    skinning::SetPreference({skinning::Mode::Cpu});
    app->TransparentMaterialSortPrecision() = TransparentSortPrecision::Off;
    std::filesystem::create_directories(captureDirectory);
    const auto frame = [&](bool ground, int mode, bool blend, float brightness) {
        app->EditMode() = ViewportEditMode::Camera;
        app->CameraPositionX() = app->ViewOffsetX() = 0.f;
        app->CameraPositionY() = app->ViewOffsetY() = 8.f;
        app->CameraPositionZ() = 0.f; app->CameraDistance() = -52.f;
        app->CameraYaw() = app->CameraRoll() = 0.f; app->CameraPitch() = .4f;
        app->LightDirection()[0] = -.5f; app->LightDirection()[1] = -1.f; app->LightDirection()[2] = .5f;
        app->GroundShadowEnabled() = ground;
        app->ProjectedShadowBlendEnabled() = blend;
        app->SetProjectedShadowAmbient(brightness);
        app->SelfShadowMode() = mode;
        app->state.selfShadowEnabled = mode != 0;
        captureRequested = true;
        app->DeltaTime() = 1.f / 30; app->state.messageSeen = 10; FrameDriver(app);
        Check(!captureRequested && !capturedPixels.empty(), "capture ground-shadow frame");
    };
    for (int i = 0; i < 8; ++i) frame(false, 0, false, 1.f);
    for (bool reset : {false, true}) {
        if (reset) Check(PostDeviceReset(app), "reset ground-shadow device");
        for (int mode : {0, 2}) for (bool blend : {false, true}) for (float brightness : {1.f, .5f}) {
            const auto name = std::string("reset-") + std::to_string(reset) + "-mode-" + std::to_string(mode) +
                "-blend-" + std::to_string(blend) + "-brightness-" + std::to_string(int(brightness * 100));
            std::vector<std::uint32_t> first;
            std::vector<std::uint32_t> withoutShadow;
            for (int stage = 0; stage < 3; ++stage) {
                const bool enabled = stage != 1;
                // Warm the MME parameter cache before comparing toggles.
                frame(enabled, mode, blend, brightness);
                frame(enabled, mode, blend, brightness);
                const auto path = captureDirectory / (name + (enabled ? (stage == 0 ? "-on" : "-restored") : "-off") + ".ppm");
                std::ofstream file(path, std::ios::binary);
                file << "P6\n" << captureWidth << ' ' << captureHeight << "\n255\n";
                for (const auto pixel : capturedPixels) {
                    const char rgb[] = {char(pixel >> 16), char(pixel >> 8), char(pixel)};
                    file.write(rgb, 3);
                }
                Check(bool(file), "save actual ground-shadow pixels");
                if (stage == 0) first = capturedPixels;
                if (stage == 1) withoutShadow = capturedPixels;
                if (stage == 2) {
                    // Ignore model shading and MSAA silhouette edges. Only
                    // compare flat shadow interiors over clear background.
                    unsigned compared = 0, changed = 0;
                    for (unsigned y = 2; y + 2 < captureHeight; ++y) for (unsigned x = 2; x + 2 < captureWidth; ++x) {
                        const std::size_t i = std::size_t(y) * captureWidth + x;
                        if (withoutShadow[i] != 0xFFFFFFu || first[i] == 0xFFFFFFu) continue;
                        bool interior = true;
                        for (const auto j : {i - 2, i + 2, i - 2 * captureWidth, i + 2 * captureWidth})
                            interior &= withoutShadow[j] == 0xFFFFFFu && first[j] == first[i];
                        if (interior) { ++compared; changed += first[i] != capturedPixels[i]; }
                    }
                    std::printf("GROUND RESTORE compared=%u changed=%u\n", compared, changed);
                    Check(compared > 100 && changed == 0, "ground-shadow toggle restores identical interior pixels");
                }
            }
            std::printf("GROUND %s size=%ux%u\n", name.c_str(), captureWidth, captureHeight);
        }
    }
    MH_DisableHook(vtable[17]);
    Check(!IsWindowVisible(app->MainWindow()), "ground-shadow host stays hidden");
}
