#pragma once

namespace mikudancestudio {
class MMDApp;
class D3DRenderer;

// Queue during device setup, start after the first successful presentation.
// Only the compiler runs on a worker; all device calls remain on the UI thread.
// MME requires the original synchronous resource entry point before BeginScene.
bool QueueDefaultEffect(D3DRenderer* renderer, int resource);
void StartDefaultEffectCompilation(D3DRenderer* renderer);
void UpdateDefaultEffect(MMDApp* app);
void CancelDefaultEffectCompilation(D3DRenderer* renderer);
}
