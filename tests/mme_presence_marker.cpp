// Exercise the host's MMEffect.dll detection without loading a renderer hook.
// This marker is only a branch regression fixture, not an MME implementation.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
