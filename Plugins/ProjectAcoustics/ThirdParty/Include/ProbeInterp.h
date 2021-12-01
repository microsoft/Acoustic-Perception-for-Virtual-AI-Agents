#pragma once
#include "MemoryOverrides.h"

namespace TritonRuntime
{
    const int MaxInterpProbes = 16;

    enum class ProbeInterpInfo
    {
        Used,
        Unassigned,
        RejectedByGeometricTests,
        RejectedInWall,
        RejectedWeightTooSmall,
        RejectedTooMany,
        RejectedComputeParamsFailed,
        ProbeNotLoaded,
        ProbeLoadFailed,
        ProbeBakeFailed
    };

    struct ProbeInterpVals
    {
        TRITON_PREVENT_HEAP_ALLOCATION;

    public:
        int ProbeIndex;
        float weight;
        ProbeInterpInfo info;
        ProbeInterpVals();
    };
} // namespace TritonRuntime
