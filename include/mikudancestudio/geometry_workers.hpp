#pragma once
#ifdef _OPENMP
#include <omp.h>
#endif

namespace mikudancestudio {
// Use the same bounded team for skinning and sorting. Alternating a full
// machine team with a smaller team costs more than these short jobs save.
inline int GeometryWorkerCount() {
#ifdef _OPENMP
    const int requested = omp_get_max_threads();
    return requested < 8 ? requested : 8;
#else
    return 1;
#endif
}
}
