#pragma once
#include "MemoryOverrides.h"
#include "TritonVector.h"

namespace TritonRuntime
{
    struct ReceiverInterpolationWeights
    {
        TRITON_PREVENT_HEAP_ALLOCATION;

    public:
        static const int MaxInterpSamples = 8;

        static const Triton::Vec3i InterpBoxCornerOffsets[MaxInterpSamples];

        bool SampleValid[MaxInterpSamples];
        float weight[MaxInterpSamples];
        Triton::Vec3i MinCornerSampleCell3DIndex;

        ReceiverInterpolationWeights();

        void Clear();

        int Count();

        void operator=(int value);
    };
} // namespace TritonRuntime
