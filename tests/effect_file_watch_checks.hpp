// Included inside mme_display_tests.cpp's fixture namespace.
void CheckEffectFileWatch(MMDApp* app, const std::filesystem::path& path) {
    Check(emdEffectCompiles > 0, "watch fixture initially compiled through native MME");
    auto* menu = GetMenu(app->MainWindow());
    const auto enabled = [&] {
        const auto state = GetMenuState(menu, 40001, MF_BYCOMMAND);
        Check(state != UINT(-1), "native MME auto-reload menu exists");
        return (state & MF_CHECKED) != 0;
    };
    if (!enabled()) SendMessageW(app->MainWindow(), WM_COMMAND, 40001, 0);
    Check(enabled(), "native MME auto-reload enabled");
    void** vtable = *reinterpret_cast<void***>(app->Renderer()->device);
    Check(MH_CreateHook(vtable[17], &ObservePresent, reinterpret_cast<void**>(&originalPresent)) == MH_OK &&
          MH_EnableHook(vtable[17]) == MH_OK, "observe reloaded effect pixels");
    const auto frame = [&] {
        app->DeltaTime() = 1.f / 30; app->state.messageSeen = 10; FrameDriver(app);
        MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    };
    const auto renderFor = [&](unsigned ms) {
        const auto end = GetTickCount64() + ms;
        do { frame(); Sleep(5); } while (GetTickCount64() < end);
    };
    const auto pixels = [&](unsigned color) {
        captureRequested = true; frame();
        Check(!captureRequested && std::count(capturedPixels.begin(), capturedPixels.end(), color) > 50,
              "actual rendered color matches current effect");
    };
    renderFor(250); pixels(0xff00ff);
    const auto before = effect_file_watch::GetStats();
    const auto compiled = emdEffectCompiles;
    renderFor(600);
    const auto idle = effect_file_watch::GetStats();
    Check(idle.files && idle.directories && idle.hits > before.hits && idle.reads == before.reads,
          "enabled auto-reload makes no repeated file opens while unchanged");
    Check(emdEffectCompiles == compiled, "unchanged effect does not recompile");
    const auto write = [&](const char* color, bool replace) {
        const auto target = replace ? std::filesystem::path(path.wstring() + L".tmp") : path;
        std::ofstream file(target, std::ios::binary | std::ios::trunc);
        file << "float4x4 WorldViewProjection : WORLDVIEWPROJECTION;\n"
                "float4 VS(float4 p : POSITION) : POSITION { return mul(p, WorldViewProjection); }\n"
                "float4 PS() : COLOR0 { return float4(" << color << "); }\n"
                "technique Main < string MMDPass = \"object\"; > { pass P { CullMode = None; "
                "AlphaBlendEnable = false; VertexShader = compile vs_2_0 VS(); "
                "PixelShader = compile ps_2_0 PS(); } }\n";
        file.close(); Check(bool(file), "save owned effect fixture");
        if (replace) Check(MoveFileExW(target.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING),
                           "editor-style atomic effect replacement");
    };
    const auto awaitReload = [&](unsigned previous, const char* label) {
        const auto begin = GetTickCount64();
        do { frame(); Sleep(5); } while (emdEffectCompiles == previous && GetTickCount64() - begin < 2000);
        Check(emdEffectCompiles > previous, label);
        std::printf("WATCH_RELOAD latency_ms=%llu case=%s\n", GetTickCount64() - begin, label);
    };
    write("0, 1, 0, 1", false); awaitReload(compiled, "normal save reloads automatically"); pixels(0x00ff00);
    SendMessageW(app->MainWindow(), WM_COMMAND, 40001, 0); Check(!enabled(), "native auto-reload can be disabled");
    const auto stopped = emdEffectCompiles;
    write("1, 0, 0, 1", true); renderFor(350);
    Check(emdEffectCompiles == stopped, "disabled auto-reload does not compile changed file"); pixels(0x00ff00);
    SendMessageW(app->MainWindow(), WM_COMMAND, 40001, 0); Check(enabled(), "native auto-reload can be re-enabled");
    awaitReload(stopped, "re-enable observes edit made while disabled"); pixels(0xff0000);
    const auto replaced = emdEffectCompiles;
    write("0, 0, 1, 1", true); awaitReload(replaced, "atomic replacement reloads automatically"); pixels(0x0000ff);
    const auto after = effect_file_watch::GetStats();
    std::printf("WATCH_STATS reads=%llu hits=%llu notifications=%llu files=%u directories=%u\n",
        after.reads, after.hits, after.notifications, after.files, after.directories);
    Check(!IsWindowVisible(app->MainWindow()), "watch fixture stays hidden");
    MH_DisableHook(vtable[17]);
}
