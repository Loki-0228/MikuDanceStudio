#ifndef MMV_LAYER_BAKE_PUBLIC_API_H
#define MMV_LAYER_BAKE_PUBLIC_API_H
#include "mmv_deformer.h"
#define MMV_LAYER_BAKE_ABI_VERSION 1u
#ifdef __cplusplus
extern "C" {
#endif
/* MMV.dll exports MMVGetLayerBakeApi. All operations use the existing plugin
 * instance. The host warns before BEGIN. Sampling covers [maximum layer..x]
 * in descending order, inclusive, and never includes MME (-1).
 * Stage removal is valid only inside MMV_MESH_BAKE_BEGIN..COMMIT/CANCEL.
 * It removes the selected objects' assignments on [x..maximum] permanently,
 * including explicit removals that override automatic attachment defaults.
 * The host must have prepared recoverable native keys/cache before COMMIT. */
typedef struct MmvLayerBakeRemoval {
    uint32_t structureSize, layer;
    const uint64_t* objectIds; uint32_t objectCount;
} MmvLayerBakeRemoval;
typedef struct MmvLayerBakeApi {
    uint32_t structureSize, abiVersion;
    /* Null layers/capacity=0 obtains required count. Descending, includes 0. */
    int (MMV_CALL *layers)(void*,uint32_t* layers,uint32_t capacity,uint32_t* count);
    int (MMV_CALL *beginSampling)(void*,uint32_t layer);
    void (MMV_CALL *endSampling)(void*);
    int (MMV_CALL *stageRemoval)(void*,const MmvLayerBakeRemoval*);
    int (MMV_CALL *querySnapshot)(void*,const MmvDeformerSnapshotQuery*,MmvDeformerSnapshot*);
    void (MMV_CALL *releaseSnapshot)(MmvDeformerSnapshot*);
} MmvLayerBakeApi;
typedef int (MMV_CALL *MmvGetLayerBakeApiFn)(uint32_t,MmvLayerBakeApi*);
#ifdef __cplusplus
}
#endif
#endif
