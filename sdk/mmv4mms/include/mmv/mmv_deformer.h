#ifndef MMV_DEFORMER_PUBLIC_API_H
#define MMV_DEFORMER_PUBLIC_API_H
#include "mmv.h"

/* Independent C ABI. Zero-initialize every structure, then set structureSize.
 * Matrices are row-major, row-vector. Strings are UTF-8. No pointer transfers
 * ownership. Inputs live only during evaluate; candidate arrays live until
 * commit/discard. Modules must catch all exceptions before crossing this ABI.
 * MmvObject and MmvObjectOutput retain their existing ABI and array strides. */
#define MMV_DEFORMER_ABI_VERSION 1u
#define MMV_DEFORMER_NO_INDEX 0xffffffffu
#ifdef __cplusplus
extern "C" {
#endif

enum MmvDeformerResult {
    MMV_DEFORMER_OK=0, MMV_DEFORMER_UNSUPPORTED_CAPABILITY=1,
    MMV_DEFORMER_INVALID_OUTPUT=2, MMV_DEFORMER_SOLVER_FAILED=3,
    MMV_DEFORMER_STALE_INPUT=4, MMV_DEFORMER_INVALID_ARGUMENT=5
};
enum MmvDeformerChannel {
    MMV_DEFORMER_MESH=1u, MMV_DEFORMER_BONES=2u,
    MMV_DEFORMER_BINDING=4u, MMV_DEFORMER_RIGIDS=8u,
    MMV_DEFORMER_JOINTS=16u, MMV_DEFORMER_SHAPES=32u,
    MMV_DEFORMER_MORPHS=64u, MMV_DEFORMER_ALL_CHANNELS=127u
};
enum MmvDeformerMode { MMV_DEFORMER_PRESERVE=0, MMV_DEFORMER_REPLACE=1, MMV_DEFORMER_CLEAR=2 };
enum MmvDeformerShapeType {
    MMV_DEFORMER_SPHERE=0, MMV_DEFORMER_BOX=1, MMV_DEFORMER_CAPSULE=2,
    MMV_DEFORMER_CONVEX=3, MMV_DEFORMER_COMPOUND=4
};
enum MmvDeformerFlag {
    MMV_DEFORMER_STATEFUL=1u, MMV_DEFORMER_ADVANCES_TIME=2u,
    MMV_DEFORMER_NATIVE_PHYSICS=4u, MMV_DEFORMER_ZERO_ONLY=8u,
    MMV_DEFORMER_MESH_ONLY=16u, MMV_DEFORMER_CROSS_OBJECT_JOINTS=32u,
    MMV_DEFORMER_GPU_STATE=64u
};
enum MmvDeformerScope { MMV_DEFORMER_SCOPE_OBJECT=1u, MMV_DEFORMER_SCOPE_BONE=2u, MMV_DEFORMER_SCOPE_MATERIAL=4u };
enum MmvDeformerObjectFlag { MMV_DEFORMER_STRUCTURE_SYNCHRONIZED=1u };
enum MmvDeformerInputFlag {
    MMV_DEFORMER_SOURCE_VALID=1u, MMV_DEFORMER_UPSTREAM_VALID=2u,
    MMV_DEFORMER_PREVIOUS_VALID=4u, MMV_DEFORMER_PLAYING=8u,
    MMV_DEFORMER_EDITING=16u, MMV_DEFORMER_RESET_PARAMETERS=32u,
    MMV_DEFORMER_NATIVE_INPUT_READY=64u
};
enum MmvDeformerResetReason {
    MMV_DEFORMER_RESET_FIRST=0, MMV_DEFORMER_RESET_SEEK=1,
    MMV_DEFORMER_RESET_SCENE=2, MMV_DEFORMER_RESET_OBJECT=3,
    MMV_DEFORMER_RESET_STRUCTURE=4, MMV_DEFORMER_RESET_ENABLE=5,
    MMV_DEFORMER_RESET_LAYER=6, MMV_DEFORMER_RESET_PARAMETERS_CHANGED=7,
    MMV_DEFORMER_RESET_DEVICE=8
};
enum MmvDeformerStage {
    MMV_DEFORMER_SOURCE=0, MMV_DEFORMER_UPSTREAM=1,
    MMV_DEFORMER_PREVIOUS_COMMITTED=2, MMV_DEFORMER_OUTPUT=3
};
typedef struct MmvDeformerCapabilities {
    uint32_t structureSize, abiVersion, readChannels, writeChannels;
    uint32_t shapeMask, scopes, flags, reserved;
} MmvDeformerCapabilities;
/* Parallel to MmvObject.bones. A key is identity; name and index are not.
 * Generated keys: <producer instance key>/bone/<module local key>. The same
 * producer/type/local-key convention applies to rigid, joint and shape keys.
 * sourceKey is empty only when no unique original item exists. */
typedef struct MmvDeformerBone {
    uint32_t structureSize, generated;
    const char* key; const char* sourceKey;
    float bindWorld[16], inverseBindWorld[16], skinMatrix[16];
} MmvDeformerBone;
typedef struct MmvDeformerRigid {
    uint32_t structureSize, generated;
    const char* key; const char* sourceKey;
    uint32_t shapeIndex, reserved;
    /* Symmetric COM-local inertia tensor; dynamic bodies require positive
     * mass and a positive-definite tensor. bodyToBone maps COM-local to the
     * linked bone's local space, matching MmvRigidBinding.boneToBody. */
    float inertia[9], bodyToBone[16];
} MmvDeformerRigid;
typedef struct MmvDeformerJoint {
    uint32_t structureSize, generated;
    const char* key; const char* sourceKey;
    float frameA[16], frameB[16], damping[6];
    uint32_t collideConnected, reserved;
    /* Empty object keys mean this object and the old dense rigidA/rigidB.
     * Cross-object ends specify BOTH keys, never another object's index. */
    const char* objectA; const char* rigidA;
    const char* objectB; const char* rigidB;
} MmvDeformerJoint;
typedef struct MmvDeformerShapeChild {
    uint32_t structureSize, shapeIndex;
    float local[16];
} MmvDeformerShapeChild;
typedef struct MmvDeformerPoint { float position[3]; } MmvDeformerPoint;
typedef struct MmvDeformerShape {
    uint32_t structureSize, type;
    const char* key; const char* sourceKey;
    uint64_t version;
    float size[3]; uint32_t generated;
    const MmvDeformerPoint* vertices; uint32_t vertexCount;
    const uint32_t* indices; uint32_t indexCount;
    const MmvDeformerShapeChild* children; uint32_t childCount;
} MmvDeformerShape;
typedef struct MmvDeformerObject {
    uint32_t structureSize, flags;
    const char* key;
    /* structureVersion covers topology connectivity/material identity and
     * binding/constraint structure. Pose-only changes leave it unchanged.
     * meshVersion covers visible mesh/material values, including geometry. */
    uint64_t structureVersion, meshVersion;
    MmvObject object;
    const MmvDeformerBone* boneInfo; uint32_t boneInfoCount;
    const MmvDeformerRigid* rigidInfo; uint32_t rigidInfoCount;
    const MmvDeformerJoint* jointInfo; uint32_t jointInfoCount;
    const MmvDeformerShape* shapes; uint32_t shapeCount;
    /* Layer binding table, independent of object.originalVertices, which
     * always denotes immutable host origin. Every render vertex has a
     * vertexBindings entry into this table; a generated render vertex may
     * have sourceVertex==NO_INDEX and still have a valid layer binding.
     * bindingVertices are bind geometry; deformedVertices are morph-deformed,
     * unskinned geometry; bindingWeights address this layer's bone table.
     * deformedVertices may be null when the host cannot provide pre-skinning
     * geometry. Consumers needing it must reject that missing capability;
     * bind geometry must not be substituted as if morphs were all zero. */
    const MmvVertex* bindingVertices;
    const MmvPhysicsVertex* deformedVertices;
    const MmvWeight* bindingWeights;
    uint32_t bindingCount;
    const uint32_t* vertexBindings; uint32_t vertexBindingCount;
} MmvDeformerObject;
typedef struct MmvDeformerScene {
    uint32_t structureSize, objectCount;
    const MmvDeformerObject* objects;
    uint64_t revision;
} MmvDeformerScene;
typedef struct MmvDeformerSelection {
    uint32_t structureSize, scope;
    const char* objectKey; const char* itemKey;
    const char* effectKey;
    uint32_t parameterBegin, parameterCount; /* Slice of input.parameters. */
} MmvDeformerSelection;
typedef struct MmvDeformerParameter {
    const char* key;
    uint32_t type, components;
    double value[4];
} MmvDeformerParameter;
typedef struct MmvDeformerCreateInfo {
    uint32_t structureSize, abiVersion, layer, reserved;
    const char* effectKey; const char* domainKey;
    const wchar_t* effectPath;
} MmvDeformerCreateInfo;
typedef struct MmvDeformerInput {
    uint32_t structureSize, abiVersion, flags, layer;
    const char* effectKey; const char* domainKey;
    uint64_t evaluationId, inputRevision, stateEpoch, simulationTick;
    double fixedStepSeconds, timeSeconds, unconsumedSeconds;
    uint32_t stepCount, reserved;
    MmvDeformerScene source, upstream, previousCommitted;
    const MmvDeformerSelection* selections; uint32_t selectionCount;
    const MmvDeformerParameter* parameters; uint32_t parameterCount;
    uint64_t configurationVersion;
    /* Host scene gravity in MMD world units, before each selection's scale. */
    float gravity[3]; uint32_t reservedGravity;
} MmvDeformerInput;
typedef struct MmvDeformerDiagnostic {
    uint32_t structureSize, code, severity, layer;
    const char* objectKey; const char* effectKey; const char* domainKey;
    const char* itemKey; const char* message;
} MmvDeformerDiagnostic;
typedef struct MmvDeformerStatistics {
    double preprocessingMilliseconds, advanceMilliseconds, unconsumedSeconds;
    uint32_t iterations, substeps;
} MmvDeformerStatistics;
typedef struct MmvDeformerObjectOutput {
    uint32_t structureSize;
    /* Preserve ignores that channel's candidate pointers; Clear empties it;
     * Replace supplies the complete channel, including parallel metadata.
     * The entire write set is indivisible when competing with another FX. */
    uint32_t mesh, bones, binding, rigids, joints, shapes, morphs;
    MmvDeformerObject value;
} MmvDeformerObjectOutput;
typedef struct MmvDeformerOutput {
    uint32_t structureSize, objectCount;
    void* candidate;
    const MmvDeformerObjectOutput* objects;
    uint32_t readChannels, writeChannels;
    MmvDeformerStatistics statistics;
    const MmvDeformerDiagnostic* diagnostics; uint32_t diagnosticCount;
} MmvDeformerOutput;
typedef struct MmvDeformerApi {
    uint32_t structureSize, abiVersion;
    int (MMV_CALL *getCapabilities)(MmvDeformerCapabilities*);
    int (MMV_CALL *create)(const MmvDeformerCreateInfo*, void**);
    int (MMV_CALL *evaluate)(void*, const MmvDeformerInput*, MmvDeformerOutput*);
    /* commit/discard/reset/destroy are noexcept by contract. commit cannot
     * allocate or fail. Both commit and discard release candidate memory. */
    void (MMV_CALL *commit)(void*, void* candidate);
    void (MMV_CALL *discard)(void*, void* candidate);
    void (MMV_CALL *reset)(void*, uint32_t reason, uint64_t stateEpoch);
    void (MMV_CALL *destroy)(void*);
} MmvDeformerApi;
typedef int (MMV_CALL *MmvDeformerGetApiFn)(uint32_t, MmvDeformerApi*);

/* MMV.dll's separate optional inspection export is
 * MMVGetDeformerInspectionApi(version, api). It uses the existing plugin
 * instance handle; it does not create or evaluate a solver. */
typedef struct MmvDeformerSnapshotQuery {
    uint32_t structureSize, layer, stage, reserved;
    const char* effectKey; const char* domainKey; const char* objectKey;
} MmvDeformerSnapshotQuery;
typedef struct MmvDeformerSnapshot {
    uint32_t structureSize, flags;
    void* lease;
    MmvDeformerScene scene;
    uint64_t evaluationId, inputRevision, stateEpoch, simulationTick;
    double timeSeconds;
    MmvDeformerStatistics statistics;
    const MmvDeformerDiagnostic* diagnostics; uint32_t diagnosticCount;
} MmvDeformerSnapshot;
typedef struct MmvDeformerInspectionApi {
    uint32_t structureSize, abiVersion;
    int (MMV_CALL *querySnapshot)(void*, const MmvDeformerSnapshotQuery*, MmvDeformerSnapshot*);
    void (MMV_CALL *releaseSnapshot)(MmvDeformerSnapshot*);
} MmvDeformerInspectionApi;
typedef int (MMV_CALL *MmvGetDeformerInspectionApiFn)(uint32_t, MmvDeformerInspectionApi*);
#ifdef __cplusplus
}
#endif
#endif
