#pragma once
#include <cstdint>
#include <string>

namespace mikudancestudio::nvidia_antialias_fix {
struct State {
    bool available = false;
    bool enabled = false;
    std::wstring error;
    int driverStatus = 0;
};
// Reads this executable's effective setting, including inherited global values.
State Inspect();
// Startup detection only. Never writes driver settings or requests UAC. AMD and
// other vendors return without loading NVAPI. Failure is nonfatal to rendering.
bool EnsureForAdapter(std::uint32_t vendorId, std::wstring& error);
// Rechecks the actual driver value for the selected NVIDIA adapter. A Data record
// cannot hide a disabled driver setting. Non-NVIDIA adapters never offer this.
bool ShouldOfferInstall();
// Called only by the explicit Help menu command. owner is the host HWND.
// Shows the administrator explanation once and elevates the standalone helper.
// Language follows the host: Japanese = 0, English = 1, Chinese = 2.
bool InstallForCurrentExecutable(void* owner, std::wstring& error, int language = 2);
}
