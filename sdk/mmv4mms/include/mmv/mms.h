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

#define MMV_MMS_SELF_SHADOW_VERSION 1u
/* MMS native R32F map: red = clip.z / clip.w; UV = clip.xy / clip.w
 * * (0.5, -0.5) + 0.5. The matrix maps scene coordinates (object.world
 * precedes it), without the camera/orbit transform. Mode is native 1 or 2.
 * Borrowed on MmvMmsFrame.device until render returns, never CPU read back.
 * The plugin releases all retained GPU references on MMV_DEVICE_LOST. */
#define MMV_MMS_SHADOW_NATIVE_PROJECTED_DEPTH 1u
typedef struct MmvMmsSelfShadow {
    uint32_t structureSize, version, encoding, mode;
    uint32_t width, height;
    uint64_t frameId, texture; /* IDirect3DTexture9, same device, R32F. */
    float lightViewProjection[16];
} MmvMmsSelfShadow;

/* Optional receiver contract, same projection/frame/device/encoding as the
 * native map. Version 1 can provide a separate default-material fine map.
 * Version 2 shares the final depth map with nativeSelfShadow and authored FX:
 * enhanced=0, mapSize.xy=actual texture size, zw=0 (never filter twice).
 * prefiltered=1 requests the same two-pass positive cubic prefilter after a
 * plugin geometry rebuild, using coarseSize and the actual map dimensions.
 * This changes depth sampling only; authored FX keep their receiver algorithm.
 * Old-sized frames omit this record. Version 1 records omit its new tail. */
#define MMV_MMS_SHADOW_SAMPLING_VERSION 2u
#define MMV_MMS_SHADOW_SAMPLING_VERSION_LEGACY 1u
typedef struct MmvMmsShadowSampling {
    uint32_t structureSize, version, enhanced, reserved;
    uint64_t frameId, texture;
    float mapSize[4];
    uint32_t prefiltered; /* Version 2 tail; zero for an unfiltered map. */
    float coarseSize[2];
} MmvMmsShadowSampling;

/* Native main-color material grouping. Original is one authored-order draw;
 * Coverage adds only an alpha==255 prepass before the translucent draw.
 * Custom FX keep their own pass states and never receive Coverage. */
#define MMV_MMS_MATERIAL_ORIGINAL 0u
#define MMV_MMS_MATERIAL_OPAQUE 1u
#define MMV_MMS_MATERIAL_TRANSLUCENT 2u
#define MMV_MMS_MATERIAL_COVERAGE 3u
#define MMV_MMS_MATERIAL_CUSTOM_EFFECT 1u
typedef int (MMV_CALL *MmvMmsReadMaterialPassFn)(void* context,
    uint32_t objectIndex, const MmvMaterial* evaluatedMaterial,
    uint32_t effectFlags, uint32_t* colorPass);
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
    /* Optional ABI 1 tail. Null/absent retains the legacy plugin-generated
       map. A supplied record with mode 0 explicitly disables self-shadow. */
    const MmvMmsSelfShadow* nativeSelfShadow;
    /* Optional ABI 1 tail. Invoked synchronously during render, using the
       evaluated material rather than its sourceMaterial index. Neither the
       callback nor its context may be retained after render returns. */
    void* materialPassContext;
    MmvMmsReadMaterialPassFn readMaterialPass;
    /* Optional ABI 1 tail. Old-sized frames retain the old plugin receiver.
       A supplied record selects the matching MMS depth-first receiver. */
    const MmvMmsShadowSampling* nativeShadowSampling;
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
