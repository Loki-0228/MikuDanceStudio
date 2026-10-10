#pragma once
#include "mmv.h"

/* MMV4MMS: synchronous, read-only scene input; no node or MMN host ABI.
 * Call render outside BeginScene. The host preserves its DX9 device state.
 * Color is a borrowed IDirect3DTexture9 on input.device, valid until the next
 * render/device event/destroy. The host may AddRef it only until device loss.
 * The DLL never resets/presents the host device or writes into input meshes. */
#define MMV_MMS_ABI_VERSION 1u
#define MMV_MMS_RELOAD_EFFECTS 1001u
#ifdef __cplusplus
extern "C" {
#endif
typedef struct MmvRenderColoredVertex { float position[3]; uint32_t argb; } MmvRenderColoredVertex;
typedef struct MmvRenderSceneObject { uint32_t objectIndex, order, castsGround; } MmvRenderSceneObject;
typedef struct MmvRenderDecorations {
    uint32_t structureSize;
    uint32_t gridEnabled, nativeStencil, pmxEdgeGate, accessorySplit;
    uint32_t groundShadowEnabled, transparentGroundShadow;
    float world[16], groundProjection[16];
    float groundColor[4], groundAmbient[4], sceneLightAmbient[4];
    uint32_t ambientArgb, colorVertex, lighting;
    uint32_t diffuseMaterialSource, ambientMaterialSource, specularMaterialSource, emissiveMaterialSource;
    uint32_t modelGroundTextureArgb;
    const MmvRenderColoredVertex* lines; uint32_t lineCount;
    const MmvRenderColoredVertex* plane; uint32_t planeCount;
    const MmvRenderSceneObject* objects; uint32_t objectCount;
} MmvRenderDecorations;
typedef struct MmvRenderMaterialChannels {
    uint32_t objectIndex, materialIndex;
    float textureMultiply[4], textureAdd[4], sphereMultiply[4], sphereAdd[4], toonMultiply[4], toonAdd[4];
} MmvRenderMaterialChannels;
typedef struct MmvMmsFrame {
    uint32_t structureSize, abiVersion;
    uint64_t device;
    MmvFrame scene;
    float background[4];
    uint32_t shadowMode, shadowWidth, shadowHeight;
    const MmvRenderDecorations* decorations;
    const MmvRenderMaterialChannels* materialChannels; uint32_t materialChannelCount;
    /* Optional ABI 1 tail. Copy raw panel values, including full-turn rotations.
       Older callers omit this tail and retain matrix-based fallback controls. */
    const MmvAccessoryControl* accessoryControls; uint32_t accessoryControlCount;
} MmvMmsFrame;
typedef struct MmvMmsOutput {
    uint32_t structureSize, width, height, reserved;
    uint64_t color, frameId;
    const char* diagnostic;
} MmvMmsOutput;
typedef struct MmvMmsApi {
    uint32_t structureSize, abiVersion;
    MmvPluginApi runtime;
    int (MMV_CALL *render)(void*, const MmvMmsFrame*, MmvMmsOutput*);
} MmvMmsApi;
typedef int (MMV_CALL *MmvMmsGetApiFn)(uint32_t, MmvMmsApi*);
/* Only load the named MMV4MMSGetPluginApi export from MMV4MMS.dll. */
#ifdef __cplusplus
}
#endif
