// Exercise the production loader's error branches without a rendering device.
struct AccessoryMessageExpectation { const wchar_t* title; const wchar_t* body; };
std::vector<AccessoryMessageExpectation> accessoryMessages;
std::size_t accessoryMessagesSeen = 0;
bool accessoryMessagesCorrect = true;
LRESULT CALLBACK InspectAccessoryMessage(int code, WPARAM wp, LPARAM lp) {
    if (code == HCBT_CREATEWND) {
        auto* created = reinterpret_cast<CBT_CREATEWNDW*>(lp)->lpcs;
        if (!(created->style & WS_CHILD)) {
            created->style &= ~WS_VISIBLE;
            SetWindowSubclass(reinterpret_cast<HWND>(wp), HideEdgeWindow, 1, 0);
        }
    }
    if (code == HCBT_ACTIVATE) {
        HWND dialog = reinterpret_cast<HWND>(wp);
        wchar_t cls[32]{}; GetClassNameW(dialog, cls, 32);
        if (std::wstring(cls) == L"#32770") {
            if (GetPropW(dialog, L"MMS.AccessoryRegression.Inspected")) return 1;
            SetPropW(dialog, L"MMS.AccessoryRegression.Inspected", reinterpret_cast<HANDLE>(1));
            const auto index = accessoryMessagesSeen++;
            if (index >= accessoryMessages.size()) accessoryMessagesCorrect = false;
            else {
                const auto& expected = accessoryMessages[index];
                if (Text(dialog) != expected.title || Text(GetDlgItem(dialog, 0xffff)) != expected.body) {
                    accessoryMessagesCorrect = false;
                    std::fprintf(stderr, "FAIL accessory dialog Unicode text at message %zu (ACP=%u)\n", index, GetACP());
                    const auto actual = Text(dialog) + L"\n" + Text(GetDlgItem(dialog, 0xffff));
                    std::string utf8(4 * actual.size() + 1, '\0');
                    const int bytes = WideCharToMultiByte(CP_UTF8, 0, actual.data(), static_cast<int>(actual.size()), utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
                    utf8.resize(bytes); std::fprintf(stderr, "actual: %s\n", utf8.c_str());
                }
            }
            ShowWindow(dialog, SW_HIDE);
            PostMessageW(dialog, WM_CLOSE, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}
bool AccessoryLoadMessageRegression(MMDApp* app, HWND owner) {
    auto* previous = g_Block; g_Block = app;
    const auto previousLanguage = app->EnglishUI();
    wchar_t missing[MAX_PATH]{};
    Check(GetTempPathW(MAX_PATH, missing) > 0, "resolve temporary directory");
    wcscat_s(missing, L"MMS_missing_accessory_Unicode_中文_20261004.x");
    Check(GetFileAttributesW(missing) == INVALID_FILE_ATTRIBUTES, "missing accessory fixture does not exist");
    HHOOK hook = SetWindowsHookExW(WH_CBT, InspectAccessoryMessage, nullptr, GetCurrentThreadId());
    Check(hook != nullptr, "inspect production accessory error dialogs");
    mdl::AccessoryRecord occupied{};
    bool passed = true;
    for (unsigned char language : {0, 1, 2}) {
        app->EnglishUI() = language;
        const bool english = language != 0;
        const auto title = english ? L"load accessory" : L"アクセサリ読込";
        accessoryMessages = {
            {title, english ? L"Cannot find xfile!!\n\nIf Japanese font is included in the filename,please rewrite it in English font."
                            : L"xファイルが見つかりません"},
            {title, english ? L"This is not x_file.\n\nIf Japanese font is included in the filename,please rewrite it in English font."
                            : L"正しいｘファイルではありません"}
        };
        accessoryMessagesSeen = 0; accessoryMessagesCorrect = true;
        LoadAccessoryFile(missing);
        passed = passed && accessoryMessagesCorrect && accessoryMessagesSeen == 2 && app->AccessorySlot(0) == nullptr;
        // No mesh is needed to reach the 255-slot limit.
        for (int slot = 0; slot < 255; ++slot) app->AccessorySlot(slot) = &occupied;
        accessoryMessages = {{title, english ? L"You cannot add accessory over 255" : L"追加できるアクセサリの最大数は255個までです"}};
        accessoryMessagesSeen = 0; accessoryMessagesCorrect = true;
        LoadAccessoryFile(missing);
        passed = passed && accessoryMessagesCorrect && accessoryMessagesSeen == 1;
        for (int slot = 0; slot < 255; ++slot) app->AccessorySlot(slot) = nullptr;
        std::printf("accessory load error dialogs language=%u ACP=%u\n", language, GetACP());
    }
    UnhookWindowsHookEx(hook);
    app->EnglishUI() = previousLanguage; g_Block = previous;
    if (passed) std::puts("PASS accessory missing/invalid/slot-limit Unicode dialogs and failed-load rollback");
    return passed;
}
