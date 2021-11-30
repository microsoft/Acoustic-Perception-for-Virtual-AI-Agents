// Copyright (c) Microsoft Corporation.  All rights reserved.

#pragma once

namespace TritonRuntime
{
    static const int NumAcousticParameters = 14;

    enum class ParameterID
    {
        DirectDelay = 0,
        DirectLoudness = 1,
        DirectAzimuth = 2,
        DirectElevation = 3,
        ReflectionsDelay = 4,

        ReflectionsLoudness = 5,
        ReflLoudness_Channel_0 = 6,
        ReflLoudness_Channel_1 = 7,
        ReflLoudness_Channel_2 = 8,
        ReflLoudness_Channel_3 = 9,
        ReflLoudness_Channel_4 = 10,
        ReflLoudness_Channel_5 = 11,
        EarlyDecayTime = 12,
        ReverbTime = 13
    };
} // namespace TritonRuntime