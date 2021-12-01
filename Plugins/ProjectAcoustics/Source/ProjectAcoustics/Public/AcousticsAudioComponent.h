// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "AkInclude.h"
#include "Components/SceneComponent.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "IAcoustics.h"
#include "AcousticsSecondarySource.h"
#include "AcousticsAudioComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogProjectAcoustics, Log, All);

// These params were originally in the acoustics
// audio component, but since they are being used in the acoustics runtime volume as well, so to avoid
// redundancy, I moved them into a struct that can be used in multiple places.
/**
 *	Structure that contains the various acoustics design params that can be tweaked to make the sound coming
 *	from the acoustics audio component to react to the surroundings.
 */
USTRUCT(BlueprintType, Category = "Acoustics")
struct FAcousticsDesignParams
{
    GENERATED_BODY()

    /** Range: 0 to 2: Apply a multiplier to the occlusion dB level computed from physics.
    If this multiplier is greater than 1, occlusion will be exaggerated, while values less than 1 make
    the occlusion effect more subtle, and a value of 0 disables occlusion. */
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
        meta = (UIMin = 0, ClampMin = 0, UIMax = 2, ClampMax = 2))
    float OcclusionMultiplier = 1.0f;

    /** Range: -20 to 20. Adds specified dB value to reverb level computed
      from physics. Positive values make a sound more reverberant, negative values make a
      sound more dry. */
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
        meta = (UIMin = -20, ClampMin = -20, UIMax = 20, ClampMax = 20))
    float WetnessAdjustment = 0.0f;

    /** Range: 0 to 2. Applies a multiplier to the reverb decay time from physics.
      For example, if the bake result specifies a decay time of 500 milliseconds, but this value is set
      to 2, the decay time applied to the source is 1 second. */
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
        meta = (UIMin = 0, ClampMin = 0, UIMax = 2, ClampMax = 2))
    float DecayTimeMultiplier = 1.0f;

    /** Range: -1 to 1.  The acoustics system computes a continuous
    value between 0 and 1, 0 meaning the player is fully indoors and 1 being outdoors.
     This is an additive adjustment to this value.
     Setting this to 1 will make a source always sound completely outdoors,
     while setting it to -1 will make it always sound indoors. */
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
        meta = (UIMin = -1, ClampMin = -1, UIMax = 1, ClampMax = 1))
    float OutdoornessAdjustment = 0.0f;

    /** Range: -60 to 0. Specify additional dry signal propagated
     *  in straight line from source to listener through geometry, in dB. */
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
        meta = (UIMin = -60, ClampMin = -60, UIMax = 0, ClampMax = 0))
    float TransmissionDb = -60.0f;

    /** Range: -1 to 1. Changes how wet-dry ratio changes with distance.
     * Values smaller than 0 makes the source sound drier and more intimate,
     * larger than zero makes a sound more aggressively reverberant as it moves
     * away from the listener.
     */
    UPROPERTY(
        EditAnywhere, BlueprintReadWrite, Category = "Acoustics",
        meta = (UIMin = -1, ClampMin = -1, UIMax = 1, ClampMax = 1))
    float WetRatioDistanceWarp = 0.0f;

    /** When set, this emitter's sound will be affected by dynamic openings at additional CPU cost.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool ApplyDynamicOpenings = false;

    /**
     * Constant values denoting the clamps of the members of this struct.
     */
    static const float OcclusionMultiplierMin;
    static const float OcclusionMultiplierMax;
    static const float WetnessAdjustmentMin;
    static const float WetnessAdjustmentMax;
    static const float DecayTimeMultiplierMin;
    static const float DecayTimeMultiplierMax;
    static const float OutdoornessAdjustmentMin;
    static const float OutdoornessAdjustmentMax;
    static const float TransmissionDbMin;
    static const float TransmissionDbMax;
    static const float WetRatioDistanceWarpMin;
    static const float WetRatioDistanceWarpMax;
};

UCLASS(
    hidecategories = Auto, AutoExpandCategories = (AkComponent, Acoustics), BlueprintType, Blueprintable,
    ClassGroup = Acoustics, meta = (BlueprintSpawnableComponent))
class PROJECTACOUSTICS_API UAcousticsAudioComponent : public UAkComponent
{
    GENERATED_UCLASS_BODY()

    /**
     *	The acoustics design params at begin play of this component.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FAcousticsDesignParams InitialDesignParams;

    /**
     *	Whether the acoustics design params can be overriden by acoustics runtime volumes.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool ApplyAcousticsVolumes;

    // Trigger this sound during this component's OnBeginPlay?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool PlayOnStart;

public:
    /** Show acoustic parameters in-editor */
    UPROPERTY(EditAnywhere, Category = "Acoustics|Debug Controls")
    bool ShowAcousticParameters;
#if CPP
    virtual void BeginPlay() override;
    virtual void
    TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void OnUnregister() override;

    // Do nothing on update transform -- the tick function handles all pertinent update logic
    // We need to override this function to disable the underlying AkComponent's implementation, which can break our
    // intended functionality
    virtual void OnUpdateTransform(
        EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
#endif

    /**
     *	Function to apply acoustics design params overrides for all the volumes that this component is inside when this
     *function is called.
     */
    void ApplyAcousticsDesignParamsOverrides();

    FAcousticsDesignParams* GetAcousticsDesignParams()
    {
        return &CurrentDesignParams;
    }

protected:
    /**
     *	The realtime value of the design params for this component when the game is running.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Acoustics")
    FAcousticsDesignParams CurrentDesignParams;

private:
    void SetWwiseDryPath(
        FAkAudioDevice* AudioDevice, UAkComponent* curListener, const FVector sourceLocation,
        const FVector listenerPosition, const IAcoustics& acoustics, const TritonWwiseParams& wwiseParams);
    bool ComputeReverbSends(TritonWwiseParams& emitterParams);
    float CalculateRT60Sends(float targetDecayTime, float wetnessAmplitude, int busIndex);
    float ComputeDecayTimeInterpolationWeight(float ShortDecayTime, float LongDecayTime, float TargetDecayTime);

    float LastFiltering;
    bool SetOpeningFilteringRTPC(FAkAudioDevice* AudioDevice, float filtering);

    IAcoustics* m_Acoustics;
    UAcousticsSecondarySource* m_SecondarySource;
};
