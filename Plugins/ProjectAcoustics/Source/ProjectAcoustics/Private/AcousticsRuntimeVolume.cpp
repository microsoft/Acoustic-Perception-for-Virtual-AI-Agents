// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AcousticsRuntimeVolume.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Components/BrushComponent.h"

AAcousticsRuntimeVolume::AAcousticsRuntimeVolume(const class FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    OverrideDesignParams.OcclusionMultiplier = 1.0f;
    OverrideDesignParams.WetnessAdjustment = 0.0f;
    OverrideDesignParams.DecayTimeMultiplier = 1.0f;
    OverrideDesignParams.OutdoornessAdjustment = 0.0f;
    OverrideDesignParams.WetRatioDistanceWarp = 0.0f;
    OverrideDesignParams.TransmissionDb = -60.0f;
    OverrideDesignParams.ApplyDynamicOpenings = false;
}