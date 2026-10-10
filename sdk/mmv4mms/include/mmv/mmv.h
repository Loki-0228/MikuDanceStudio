#ifndef MMV_PUBLIC_API_H
#define MMV_PUBLIC_API_H

#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

/* Versioned C ABI. No DirectX, Bullet, STL, or host-private layout types.
 * Strings are UTF-8 unless explicitly wchar_t. All input pointers are borrowed
 * through Evaluate/ReleaseOutput only. The plugin copies metadata used by UI.
 * Matrices use row vectors, row-major storage and the host's MMD coordinates.
 * Skinned vertices are in object mesh space; world converts them to MMD world.
 * Bones, rigid poses and joint anchors are in MMD world space.
 */
#define MMV_ABI_VERSION 3u
#ifdef _WIN32
#define MMV_CALL __cdecl
#else
#define MMV_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

enum MmvCapability {
    MMV_CAP_DX12_MESH = 1u, MMV_CAP_DX12_TEXTURE = 2u,
    MMV_CAP_DX9_FX = 4u, MMV_CAP_TOPOLOGY = 8u,
    MMV_CAP_MATERIALS = 16u, MMV_CAP_EDITOR = 32u,
    /* The MmvPluginApi tail carries stageBakeRange.  A host that passes the old
     * structure size never sees the slot and must not require it. */
    MMV_CAP_BAKE_RANGE = 64u,
    /* The same tail carries retime.  A host that does not find this bit must
     * tell the user to update MMV.dll; it must not silently skip the conversion. */
    MMV_CAP_FRAME_RATE_RETIME = 128u,
    MMV_CAP_DEFORMER_INPUT = 256u,
    MMV_CAP_LAYER_MESH_CACHE = 512u,
    MMV_CAP_GLOBAL_PROCESSOR = 1024u,
    /* The MmvPluginApi tail carries postProcess (MME ScriptOrder=postprocess). */
    MMV_CAP_SCENE_POSTPROCESS = 2048u
};
enum MmvInputDemand {
    /* The host always supplies object identity, paths, and display names when
     * it calls queryInputDemand. These bits select additional frame inputs. */
    MMV_INPUT_DEMAND_NONE = 0u,
    MMV_INPUT_DEMAND_VERTICES = 1u << 0,
    MMV_INPUT_DEMAND_TRIANGLES = 1u << 1,
    MMV_INPUT_DEMAND_MATERIALS = 1u << 2,
    MMV_INPUT_DEMAND_ORIGINAL_VERTICES = 1u << 3,
    MMV_INPUT_DEMAND_WEIGHTS = 1u << 4,
    MMV_INPUT_DEMAND_BONES = 1u << 5,
    MMV_INPUT_DEMAND_MORPHS = 1u << 6,
    MMV_INPUT_DEMAND_RIGIDS = 1u << 7,
    MMV_INPUT_DEMAND_JOINTS = 1u << 8,
    MMV_INPUT_DEMAND_PHYSICS = 1u << 9,
    MMV_INPUT_DEMAND_WORLD = 1u << 10,
    MMV_INPUT_DEMAND_SELF_SHADOW = 1u << 11,
    MMV_INPUT_DEMAND_ALL = (1u << 12) - 1u,
    /* Compatibility name for callers that need the conservative full frame. */
    MMV_INPUT_DEMAND_SCENE = MMV_INPUT_DEMAND_ALL
};
enum MmvObjectKind { MMV_MODEL = 0, MMV_ACCESSORY = 1 };
enum MmvNameKind {
    MMV_NAME_OBJECT=0, MMV_NAME_BONE=1, MMV_NAME_MORPH=2,
    MMV_NAME_MATERIAL=3, MMV_NAME_RIGID=4, MMV_NAME_JOINT=5
};
enum MmvEvent {
    MMV_PROJECT_OPENED = 1, MMV_PROJECT_SAVED = 2, MMV_SCENE_RESET = 3,
    MMV_OBJECT_REMOVED = 4, MMV_DEVICE_LOST = 5, MMV_DEVICE_RESTORED = 6,
    MMV_LANGUAGE_CHANGED = 7, /* notify object argument: Japanese 0, English 1, Chinese 2. */
    MMV_MESH_BAKED = 8,       /* staged only: inside a bake transaction, disable this
                               * object's bindings in the candidate.  The live and
                               * committed configurations are untouched, so a caller
                               * can stage several objects and still fail cleanly. */
    /* Mesh bake transaction.  A host that bakes object meshes on the GPU calls
     * BEGIN, then MMV_MESH_BAKED once per baked object, then COMMIT.  An older
     * plugin answers BEGIN with MMV_INVALID_ARGUMENT because the event did not
     * exist yet, so a host that does not get MMV_OK from BEGIN must report that
     * this MMV cannot disable baked mesh assignments and change no state at all.
     *
     *   BEGIN (9)  opens a transaction on an independent copy of the complete
     *              live configuration, including assignments that were discovered
     *              from per-object sidecars rather than from a saved project.
     *              Returns MMV_FAILED when a transaction is already open.
     *   BAKED (8)  stages "disabled" for every binding of the object argument
     *              inside the candidate.  MMV_INVALID_ARGUMENT when the object is
     *              unknown, MMV_FAILED when no transaction is open.
     *   COMMIT (10) installs the candidate as the complete live configuration and
     *              persists the same document as the committed project document,
     *              so every existing binding of a staged object stops running,
     *              not only the ones that came from a saved project.  No merge of
     *              a project-shaped subset happens, effects are not recompiled,
     *              and no effect file has to still exist.  MMV_FAILED when no
     *              transaction is open.  An editor refresh failure never turns a
     *              committed transaction into a failure.
     *   CANCEL (11) discards the candidate without any side effect.  Cancelling
     *              with no transaction open is MMV_OK.
     *
     * MMV_SCENE_RESET discards an open transaction. */
    MMV_MESH_BAKE_BEGIN = 9, MMV_MESH_BAKE_COMMIT = 10, MMV_MESH_BAKE_CANCEL = 11,
    MMV_AUTO_RELOAD_CHANGED = 12 /* notify object argument: 0 off, nonzero on. */
};
enum MmvResult { MMV_OK = 0, MMV_INVALID_ARGUMENT = 1, MMV_FAILED = 2, MMV_UNSUPPORTED = 3 };
enum MmvVertexFlag { MMV_VERTEX_EDGE_DISABLED = 1u };

/* GPU structured-buffer layout, mirrored by mmv.fxh. */
typedef struct MmvVertex {
    float position[3], normal[3], uv[2], additionalUv[16];
    float edgeScale;
    uint32_t sourceVertex, flags, reserved;
} MmvVertex;
typedef struct MmvTriangle { uint32_t vertex[3], material; } MmvTriangle;
typedef struct MmvWeight {
    int32_t bones[4]; float weights[4]; uint32_t type;
    float sdefCenter[3], sdefR0[3], sdefR1[3];
} MmvWeight;
typedef struct MmvBone { const char* name; float world[16]; float origin[3]; uint32_t valid; int32_t parent; } MmvBone;
typedef struct MmvMorph { const char* name; float value; uint32_t type; } MmvMorph;
typedef struct MmvRigidBody {
    const char* name;
    uint32_t valid, shape, mode, group, collisionMask;
    int32_t bone;
    float world[16], size[3], mass, linearVelocity[3], angularVelocity[3];
    float damping[2], restitution, friction;
} MmvRigidBody;
typedef struct MmvJoint {
    const char* name; uint32_t valid; int32_t rigidA, rigidB;
    float anchorA[16], anchorB[16];
    /* Per axis: lower > upper is free, lower == upper is locked, otherwise
       limited (native Bullet convention). Preserve the original bound values. */
    float linearLower[3], linearUpper[3], angularLower[3], angularUpper[3], springs[6];
} MmvJoint;
typedef struct MmvTexture {
    const wchar_t* path;
    const float* rgba; /* Optional tightly packed RGBA32F pixels. */
    uint32_t width, height;
} MmvTexture;
typedef struct MmvMaterial {
    const char* key; const char* name; const char* nameEn;
    float diffuse[4], ambient[3], specular[3], power, edgeColor[4], edgeSize;
    uint32_t flags, sphereMode;
    int32_t sourceMaterial; /* -1 for an entirely new material. */
    MmvTexture texture, sphere, toon;
} MmvMaterial;
typedef struct MmvMesh {
    const MmvVertex* vertices; uint32_t vertexCount;
    const MmvTriangle* triangles; uint32_t triangleCount;
    const MmvMaterial* materials; uint32_t materialCount;
    float boundsMin[3], boundsMax[3];
    uint64_t revision;
    uint32_t vertexCapacity, triangleCapacity;
} MmvMesh;
/* A baked mesh replaces the output at its original MMV layer, never Source.
 * The host owns these records and all mesh memory through evaluate. Positive
 * layers require synchronized structure; the host must reject a bake whose
 * non-mesh channels cannot be represented by its native scene. */
enum MmvBakedMeshFlag { MMV_BAKED_MESH_STRUCTURE_SYNCHRONIZED = 1u };
typedef struct MmvBakedMeshInput {
    uint32_t structureSize, layer;
    uint64_t objectId, sourceGeneration;
    uint32_t flags, reserved;
    MmvMesh mesh;
} MmvBakedMeshInput;
typedef struct MmvObject {
    uint32_t structureSize, kind, instance, visible;
    uint64_t id, generation;
    const wchar_t* path;
    const char* name;
    float world[16];
    MmvMesh mesh;
    const MmvVertex* originalVertices;
    const MmvWeight* weights;
    uint32_t originalVertexCount; /* Length of both original arrays, independent of output topology. */
    const MmvBone* bones; uint32_t boneCount;
    const MmvMorph* morphs; uint32_t morphCount;
    const MmvRigidBody* rigids; uint32_t rigidCount;
    const MmvJoint* joints; uint32_t jointCount;
} MmvObject;
/* Optional raw accessory panel snapshot. Rotation is in radians and may span
   multiple turns; it cannot be reconstructed from the world matrix. This
   standalone record does not change MmvObject or MmvFrame array layouts. */
typedef struct MmvAccessoryControl {
    uint32_t structureSize, reserved;
    uint64_t objectId;
    float position[3], rotation[3], scale, opacity;
} MmvAccessoryControl;
struct MmvDeformerScene;
typedef struct MmvSelfShadow {
    uint32_t structureSize, mode; /* Native self-shadow mode: 1 or 2. */
    float lightViewProjection[16]; /* Includes the host frame world transform. */
    MmvTexture depth; /* RGBA32F; red carries the native projected depth. */
} MmvSelfShadow;
typedef struct MmvFrame {
    uint32_t structureSize, objectCount;
    const MmvObject* objects;
    uint64_t frameId;
    double timeSeconds, elapsedSeconds;
    float view[16], projection[16], lightDirection[3], lightColor[3];
    uint32_t width, height;
    /* Optional ABI 1 tail. Deep-owned layer data, only when structureSize
     * reaches this field and MMV_CAP_DEFORMER_INPUT is advertised. The old
     * object array stride and the final MME-facing output layout are unchanged. */
    const struct MmvDeformerScene* layerData;
    /* Optional ABI 1 tail. Read lazily only for an object_ss technique.
     * Returned pixels remain owned by the host until evaluate returns. */
    uint32_t selfShadowMode;
    void* selfShadowContext;
    int (MMV_CALL *readSelfShadow)(void*, MmvSelfShadow*);
    /* Optional ABI tail. Per-object native MME outline inputs, indexed like
       objects[]. Kept outside MmvObject so older object-array strides remain
       valid. */
    const struct MmvObjectRenderData* objectRenderData;
    /* Optional ABI tail. Per-object accessory panel transparency (MME
       CONTROLOBJECT item "Tr", 0..1), indexed like objects[]; 1 for models.
       Null when the host does not supply it. */
    const float* objectOpacity;
} MmvFrame;
typedef struct MmvObjectRenderData {
    float outlineDistance;
    uint32_t flags;
} MmvObjectRenderData;
enum MmvObjectRenderFlag { MMV_OBJECT_RENDER_PMX = 1u };
/* Scene post-processing (MMV_CAP_SCENE_POSTPROCESS).  MME post effects
 * (ScriptClass=scene, ScriptOrder=postprocess) filter the finished scene, which
 * does not exist yet when evaluate runs.  After evaluate the host draws its
 * scene as usual, then calls postProcess(plugin, NULL, &out): out.pending is
 * nonzero when the last evaluate queued post effects.  Only then the host reads
 * the viewport back and calls postProcess(plugin, &scene, &out); the returned
 * image replaces the viewport (no blending) before UI overlays are drawn.
 * scene.frameId must be the frameId of that evaluate.  Pixels are tightly
 * packed RGBA32F rows, top row first, width x height of the evaluated frame.
 * out.image stays owned by the plugin until the next evaluate or postProcess. */
typedef struct MmvSceneImage {
    uint32_t structureSize;
    uint32_t width, height;
    uint64_t frameId;
    const float* rgba;
} MmvSceneImage;
typedef struct MmvPostProcessOutput {
    uint32_t structureSize;
    uint32_t pending; /* Query: number of queued post effects. */
    uint32_t applied; /* Render: 1 when image holds the processed scene. */
    MmvTexture image;
} MmvPostProcessOutput;
typedef struct MmvObjectOutput {
    uint64_t objectId, sourceGeneration;
    MmvMesh mesh;
    uint32_t hidden;
} MmvObjectOutput;
typedef struct MmvOutput {
    uint32_t structureSize, objectCount;
    const MmvObjectOutput* objects;
    double computeMilliseconds, readbackMilliseconds;
} MmvOutput;
typedef struct MmvBinding {
    uint32_t structureSize, objectKind, instance, enabled, controller;
    const wchar_t* objectPath;
    const char* owner; const char* target; const char* material;
    const wchar_t* effectPath; /* Empty is an explicit removal. */
    const char* bone; /* Optional ABI 1 tail: index:original-name; exclusive with material. */
} MmvBinding;
/* One inclusive MMD frame range of FX instances to switch off, used only inside
 * the mesh bake transaction. Every FX instance assigned to a listed object gets
 * a native enable track over [firstFrame, lastFrame]; frames outside the range
 * keep the values the instance already had. The range must satisfy
 * 0 <= firstFrame <= lastFrame, and lastFrame must leave room for the restore
 * key at lastFrame + 1. Nothing is written when any object is unknown or any
 * part of the request is refused. */
typedef struct MmvBakeRange {
    uint32_t structureSize;
    int32_t firstFrame, lastFrame;
    const uint64_t* objectIds;
    uint32_t objectCount;
} MmvBakeRange;
/* Optional read-only physics input. No Bullet objects cross this interface.
 * The host owns every array until the end of the current evaluate call.
 * vertices contain the current morph-deformed, unskinned model geometry.
 * boneToBody maps body-local coordinates into the linked bone's coordinates.
 * Joint frames are body-local, matching the model's original constraint. */
typedef struct MmvPhysicsVertex { float position[3], normal[3]; } MmvPhysicsVertex;
typedef struct MmvRigidBinding { uint32_t valid; float boneToBody[16], inertia[3]; } MmvRigidBinding;
typedef struct MmvJointFrames { uint32_t valid; float frameA[16], frameB[16]; } MmvJointFrames;
enum MmvPhysicsFlags { MMV_PHYSICS_TIMING_VALID=1, MMV_PHYSICS_PLAYING=2, MMV_PHYSICS_FRAME_STEP=4 };
typedef struct MmvPhysicsInput {
    uint32_t structureSize, nativePhysicsEnabled;
    uint64_t objectId;
    float gravity[3]; uint32_t flags;
    const MmvPhysicsVertex* vertices; uint32_t vertexCount;
    const MmvRigidBinding* rigidBindings; uint32_t rigidCount;
    const MmvJointFrames* jointFrames; uint32_t jointCount;
    /* Optional tail: elapsed real time for same-frame editor simulation.
     * Playback/recording uses timeline time; flags distinguish a slow frame
     * from a seek. TIMING_VALID is set only when this tail is available. */
    double frameDeltaSeconds;
} MmvPhysicsInput;
/* Optional declaration parsed from an X file, or a same-name FX when MME is
 * absent. FIXED_COMPANION locks the root assignment to enabled layer zero. */
enum MmvAccessoryDeclarationFlag { MMV_ACCESSORY_DECLARED = 1u, MMV_ACCESSORY_FIXED_COMPANION = 2u };
typedef struct MmvAccessoryDeclaration {
    uint32_t structureSize, present, controller;
    const wchar_t* effectPath;
    const char* error;
} MmvAccessoryDeclaration;
enum MmvProcessorMode { MMV_PROCESSOR_AUTOMATIC = 0, MMV_PROCESSOR_GPU = 1, MMV_PROCESSOR_CPU = 2 };
typedef struct MmvProcessorInfo {
    uint32_t structureSize, mode;
    uint64_t adapterId; /* DXGI LUID, required for GPU mode; zero otherwise. */
} MmvProcessorInfo;

typedef struct MmvHostServices {
    uint32_t structureSize, abiVersion;
    void* context; void* ownerWindow;
    const wchar_t* pluginDirectory;
    void (MMV_CALL *log)(void*, uint32_t level, const char* message);
    void (MMV_CALL *requestRedraw)(void*);
    /* Optional ABI 1 tail. UTF-8 display name, valid until the next query.
     * Copy immediately. Names never replace snapshot or archive identities. */
    const char* (MMV_CALL *displayName)(void*,uint64_t objectId,uint32_t kind,uint32_t index,uint32_t language);
    /* Optional ABI 1 tail. Normal source-level service, no injection/hooks.
     * This query does not change the host's physics switch or bodies. */
    int (MMV_CALL *physicsInput)(void*,uint64_t objectId,MmvPhysicsInput*);
    /* acquire=1 selects the normal native-physics-off menu command and saves
     * the previous mode; acquire=0 restores it. Optional, explicit FX request. */
    int (MMV_CALL *selectBullet3)(void*,uint32_t acquire);
    int (MMV_CALL *accessoryDeclaration)(void*,uint64_t objectId,MmvAccessoryDeclaration*);
    /* Optional ABI 1 tail, supported by MMV_CAP_LAYER_MESH_CACHE. Return MMV_OK
     * with zero count when no cache applies to this frame. Unique object/layer
     * records are the lowest-priority mesh output of their layer, so new FX
     * can override them. The callback must not re-enter MMV. */
    int (MMV_CALL *bakedMeshes)(void*,const MmvBakedMeshInput**,uint32_t* count);
    /* Optional ABI 1 tail, announced by MMV_CAP_GLOBAL_PROCESSOR. Returns the
     * active user-wide MMV processing choice, not a pending next-launch choice.
     * Follow-renderer is resolved by the host before this call. Queried once
     * at creation. GPU mode must use this LUID; CPU mode uses the MMV CPU data path.
     * Missing service keeps legacy Auto. A reset preserves this choice. */
    int (MMV_CALL *processor)(void*,MmvProcessorInfo*);
} MmvHostServices;
/* ---------------------------------------------------------------------------
 * Frame-rate retime (optional ABI 1 tail, MMV_CAP_FRAME_RATE_RETIME).
 *
 * The host prepares a candidate for one project frame-rate correction, reads
 * the details it wants to report, and either commits or cancels.  Nothing in
 * the plugin's live configuration or committed document changes before COMMIT.
 *
 * Frames: newFrame = round(frame * currentFPS / originalFPS), anchored at 0.
 * Two input keys of one component that land on the same newFrame become one key
 * (a merge).  A merged key is reported with mergeFirst/mergeLast, the first and
 * last ORIGINAL FRAME NUMBER of the group it was folded into, so a caller can
 * name the time slots it omitted.
 *
 * Which document: the details describe the LIVE configuration, which is what the
 * panel and the render output show.  COMMIT also installs the candidate of the
 * committed project document, so saving the project cannot restore the old
 * frames.  A project without an MMV plugin never reaches this call at all: the
 * host prepares an empty plan by itself and installs nothing.
 *
 * Every string is UTF-8, borrowed for the duration of the callback only.  The
 * callback must not call back into any MMV function, because the plan it is
 * being told about is not installed yet.
 * ------------------------------------------------------------------------ */
enum MmvRetimePhase { MMV_RETIME_PREPARE = 0, MMV_RETIME_COMMIT = 1, MMV_RETIME_CANCEL = 2 };
enum MmvRetimeKind {
    /* A rounding error: the unrounded scaled frame differs from the frame the key
     * can actually take (newFrame != exactFrame).  It is the same test
     * animation_retime applies to its own roundedKeys counter, so a key whose
     * scaled frame is already an integer - every key of an integral ratio such as
     * 30 -> 60 - is NOT rounded here.  It can be reported even when the key keeps
     * its own frame number. */
    MMV_RETIME_ROUNDED = 1,
    /* The key is one member of a merged slot; mergeFirst..mergeLast are the first
     * and last original frame number of that slot.  This is independent of
     * rounding: the member that already sat on the output frame was merged but
     * not rounded, which is what an integer ratio produces. */
    MMV_RETIME_MERGED = 2
};
/* One reported REASON for one key, delivered through MmvRetimeDetailFn.  The
 * records are per reason, not per key: a key can be reported twice, once as
 * MMV_RETIME_MERGED and once as MMV_RETIME_ROUNDED, and both facts are true.
 * Only these two are reasons: an exact rescale that neither rounded a frame nor
 * collided two keys reports no record at all, because it lost nothing - the
 * conversion it performed is visible in the statistics (tracks, inputKeys,
 * outputKeys, changedInstances) and in every key's new frame in the document.
 * `frame` is the original frame, `exactFrame` the unrounded scaled frame and
 * `newFrame` the result.  `owner` is the instance owner (the suffix of the
 * "Parameters." + owner section), `object` the source object path of the
 * instance and `channel` the variable and component ("Var.<id>.<component>" or
 * "NativeEnable"). */
typedef struct MmvRetimeDetail {
    uint32_t structureSize, kind;
    int32_t frame, newFrame;
    double exactFrame;
    int32_t mergeFirst, mergeLast;
    const char* owner;
    const char* object;
    const char* channel;
    uint32_t reserved;
} MmvRetimeDetail;
typedef void (MMV_CALL *MmvRetimeDetailFn)(void* context, const MmvRetimeDetail* detail);
typedef struct MmvRetimeRequest {
    uint32_t structureSize, phase;
    double originalFPS, currentFPS;
    /* Opaque plan identity.  PREPARE writes the token of the plan it built; a
     * later COMMIT / CANCEL must pass that same token, so a stale cancel can
     * never discard a newer plan and a committed plan can never commit twice. */
    uint64_t token;
    MmvRetimeDetailFn detail;
    void* detailContext;
    uint32_t reserved;
} MmvRetimeRequest;
typedef struct MmvRetimeStats {
    uint32_t structureSize;
    /* Every counter is per VARIABLE COMPONENT TRACK: a Float4 variable
     * contributes up to four tracks, and the native enable gate is one record
     * and one track.  changedInstances counts the "Parameters." sections that
     * changed, so a caller reporting one line per instance uses it.  roundedKeys
     * counts every key whose unrounded scaled frame was not an integer, so it is
     * also the number of MMV_RETIME_ROUNDED records and it is exactly what
     * animation_retime reports for the same tracks; the MMV_RETIME_MERGED records
     * number mergedKeys + mergeGroups, because every member of a collapsed group
     * is reported while only the members after the first one are counted as
     * removed.  An exact rescale has neither, and is still visible here through
     * tracks, inputKeys, outputKeys and changedInstances.  No counter truncates a
     * report: a counter that would not fit is clamped to its maximum instead of
     * wrapping. */
    uint32_t tracks;        /* parameter component tracks visited */
    uint32_t inputKeys, outputKeys;
    uint32_t mergeGroups, mergedKeys, roundedKeys;
    uint32_t changedInstances;
    double maxSampledValueError;   /* sampled measurement, never a strict bound */
} MmvRetimeStats;
typedef struct MmvPluginApi {
    uint32_t structureSize, abiVersion, capabilities;
    const char* buildName;
    int (MMV_CALL *create)(const MmvHostServices*, void** instance);
    void (MMV_CALL *destroy)(void*);
    int (MMV_CALL *evaluate)(void*, const MmvFrame*, MmvOutput*);
    void (MMV_CALL *releaseOutput)(void*, MmvOutput*);
    int (MMV_CALL *notify)(void*, uint32_t event, const wchar_t* path, uint64_t objectId);
    int (MMV_CALL *setBinding)(void*, const MmvBinding*);
    int (MMV_CALL *load)(void*, const wchar_t* path, uint32_t merge);
    int (MMV_CALL *save)(void*, const wchar_t* path, uint64_t objectId);
    void (MMV_CALL *showEditor)(void*, void* ownerWindow, uint32_t language);
    const char* (MMV_CALL *status)(void*);
    /* Optional ABI 2 tail. Only valid inside MMV_MESH_BAKE_BEGIN .. COMMIT or
     * CANCEL: it writes the candidate document and leaves the live and committed
     * configurations alone until COMMIT. An old host that passes the previous
     * structure size does not see this slot at all; a host that passes the full
     * size must still accept a null slot. MMV_CAP_BAKE_RANGE announces it. */
    int (MMV_CALL *stageBakeRange)(void*, const MmvBakeRange*);
    /* Optional ABI 2 tail, announced by MMV_CAP_FRAME_RATE_RETIME.  One call per
     * phase:
     *   PREPARE  (request.token ignored) builds the candidate and writes its
     *            token into request.token (and the statistics into stats).  It
     *            also reports every key that moved through request.detail, but
     *            only after the whole candidate converted: a failed PREPARE
     *            reports nothing and installs nothing.  MMV_UNSUPPORTED when the
     *            plugin cannot convert at all; MMV_FAILED with `status` naming
     *            the defect when this document cannot be converted.  The host
     *            must ask the user to update MMV.dll instead of converting
     *            partially, and must not report a finished conversion.
     *   COMMIT   (request.token required) installs the candidate and refreshes
     *            the panel, the native enable and the render output.  An empty
     *            plan commits successfully.  It refuses a token that a newer
     *            PREPARE replaced and a candidate whose revision or parameter
     *            sections changed since PREPARE.
     *   CANCEL   (request.token required) discards that candidate; an unknown or
     *            already settled token is MMV_INVALID_ARGUMENT.
     * Both structures carry their size.  Every field up to
     * MmvRetimeStats::changedInstances of the request and the statistics must be
     * declared by the caller; fields past the caller's size are not written.
     * A null slot with the capability bit set is invalid.  The detail callback is
     * optional and may be null. */
    int (MMV_CALL *retime)(void*, MmvRetimeRequest*, MmvRetimeStats*);
    /* Required ABI 3 callback. `frame` contains scene identity and names, but no
     * optional mesh or physical arrays. Return the union of data categories
     * required by every assigned FX, including bindings that are disabled now
     * but can turn on at an FX keyframe. The host caches the mask until plugin
     * configuration or scene membership changes. Unknown dependencies return
     * MMV_INPUT_DEMAND_ALL. */
    int (MMV_CALL *queryInputDemand)(void*, const MmvFrame*, uint32_t* demand);
    /* Optional tail, announced by MMV_CAP_SCENE_POSTPROCESS; see MmvSceneImage.
     * A host that passes an older structure size never sees this slot. */
    int (MMV_CALL *postProcess)(void*, const MmvSceneImage* scene, MmvPostProcessOutput* out);
} MmvPluginApi;
typedef int (MMV_CALL *MmvGetPluginApiFn)(uint32_t requestedVersion, MmvPluginApi*);

#ifdef __cplusplus
}
static_assert(sizeof(MmvVertex) == 112, "MMV vertex/HLSL layout");
static_assert(sizeof(MmvTriangle) == 16, "MMV triangle/HLSL layout");
#endif
#endif
