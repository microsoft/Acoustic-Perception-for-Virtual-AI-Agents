// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AcousticsAudioComponent.h"
#include "AcousticsDynamicOpening.h"
#include "Engine/GameEngine.h"
#include "TritonWwiseParams.h"
#include "MathUtils.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"
#include <Classes/GameFramework/HUD.h>
#include <Classes/GameFramework/PlayerController.h>
#include "AcousticsRuntimeVolume.h"

DEFINE_LOG_CATEGORY(LogProjectAcoustics);

static TAutoConsoleVariable<int32>
    CVarAcousticDebug(TEXT("PA.ShowParameters"), 0, TEXT("Show debug info for Project Acoustics"));

// Initializing the values of the clamps for the acoustics design params.
// NOTE: Make sure you change the values of the clamps in the uproperty of these members if you're changing them here.
const float FAcousticsDesignParams::OcclusionMultiplierMin = 0.0f;
const float FAcousticsDesignParams::OcclusionMultiplierMax = 2.0f;
const float FAcousticsDesignParams::WetnessAdjustmentMin = -20.0f;
const float FAcousticsDesignParams::WetnessAdjustmentMax = 20.0f;
const float FAcousticsDesignParams::DecayTimeMultiplierMin = 0.0f;
const float FAcousticsDesignParams::DecayTimeMultiplierMax = 2.0f;
const float FAcousticsDesignParams::OutdoornessAdjustmentMin = -1.0f;
const float FAcousticsDesignParams::OutdoornessAdjustmentMax = 1.0f;
const float FAcousticsDesignParams::TransmissionDbMin = -60.0f;
const float FAcousticsDesignParams::TransmissionDbMax = 0.0f;
const float FAcousticsDesignParams::WetRatioDistanceWarpMin = -1.0f;
const float FAcousticsDesignParams::WetRatioDistanceWarpMax = 1.0f;

UAcousticsAudioComponent::UAcousticsAudioComponent(const class FObjectInitializer& ObjectInitializer)
    : UAkComponent(ObjectInitializer)
{
    // Do not use Wwise's occlusion/reverb behavior.
    OcclusionRefreshInterval = 0.0f;
    UseReverbVolumes(false);
    EnableSpotReflectors = 0;
#if AK_WWISESDK_VERSION_MAJOR < 2019 || (AK_WWISESDK_VERSION_MAJOR == 2019 && AK_WWISESDK_VERSION_MINOR < 2)
    bUseSpatialAudio = false;
#else
    bUseSpatialAudio_DEPRECATED = false;
#endif

    auto d = UserDesign::Default();
    // The design params need to be accessed through the
    // struct instance now.
    InitialDesignParams.OcclusionMultiplier = d.OcclusionMultiplier;
    InitialDesignParams.WetnessAdjustment = d.WetnessAdjustment;
    InitialDesignParams.DecayTimeMultiplier = d.DecayTimeMultiplier;
    InitialDesignParams.OutdoornessAdjustment = d.OutdoornessAdjustment;
    InitialDesignParams.WetRatioDistanceWarp = d.DRRDistanceWarp;
    InitialDesignParams.TransmissionDb = d.TransmissionDb;
    ShowAcousticParameters = false;
    InitialDesignParams.ApplyDynamicOpenings = false;
    // The acoustics audio component should be overridable by default.
    ApplyAcousticsVolumes = true;
    CurrentDesignParams = InitialDesignParams;
    PlayOnStart = true;

    m_Acoustics = nullptr;
    LastFiltering = -1.0f;
}

void UAcousticsAudioComponent::BeginPlay()
{
    Super::BeginPlay();

    const auto a = GetOwner();

    // Get reference to secondary source so we can extract SPL
    UAcousticsSecondarySource* sourceInfo = nullptr;
    const auto comps = a->GetComponents();
    for (const auto& c : comps)
    {
        if (c->IsA<UAcousticsSecondarySource>())
        {
            m_SecondarySource = (UAcousticsSecondarySource*)c;
            break;
        }
    }

    // cache module instance
    if (IAcoustics::IsAvailable())
    {
        m_Acoustics = &(IAcoustics::Get());
    }

    // Apply the params set in the editor UI
    CurrentDesignParams = InitialDesignParams;

    // Apply the design param overrides if the component spawns inside a volume.
    ApplyAcousticsDesignParamsOverrides();

    if (PlayOnStart)
    {
        FAkAudioDevice* AkAudioDevice = FAkAudioDevice::Get();
        if (AkAudioDevice)
        {
            AkAudioDevice->SetAttenuationScalingFactor(this, AttenuationScalingFactor);
            AkAudioDevice->PostEvent(GET_AK_EVENT_NAME(AkAudioEvent, EventName), this, 0, nullptr, nullptr);
        }
    }
}

// Function to apply the acoustics design params overrides from
// the volumes that this acoustics audio component is inside.
void UAcousticsAudioComponent::ApplyAcousticsDesignParamsOverrides()
{
    if (ApplyAcousticsVolumes)
    {
        TArray<FOverlapResult> OverlapResults;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(AddForceOverlap), false, GetOwner());
        UWorld* World = GetWorld();

        if (World && World->OverlapMultiByObjectType(
                         OverlapResults,
                         GetComponentLocation(),
                         FQuat::Identity,
                         FCollisionObjectQueryParams(ECC_WorldStatic),
                         FCollisionShape::MakeSphere(0.0f),
                         Params))
        {
            FAcousticsDesignParams NewDesignParams = InitialDesignParams;
            for (const FOverlapResult& OverlapResult : OverlapResults)
            {
                AAcousticsRuntimeVolume* AcousticsRuntimeVolume = Cast<AAcousticsRuntimeVolume>(OverlapResult.Actor);
                if (AcousticsRuntimeVolume)
                {
                    NewDesignParams.OcclusionMultiplier = FMath::Clamp(
                        (NewDesignParams.OcclusionMultiplier *
                         AcousticsRuntimeVolume->OverrideDesignParams.OcclusionMultiplier),
                        FAcousticsDesignParams::OcclusionMultiplierMin,
                        FAcousticsDesignParams::OcclusionMultiplierMax);
                    NewDesignParams.WetnessAdjustment = FMath::Clamp(
                        (NewDesignParams.WetnessAdjustment +
                         AcousticsRuntimeVolume->OverrideDesignParams.WetnessAdjustment),
                        FAcousticsDesignParams::WetnessAdjustmentMin,
                        FAcousticsDesignParams::WetnessAdjustmentMax);
                    NewDesignParams.DecayTimeMultiplier = FMath::Clamp(
                        (NewDesignParams.DecayTimeMultiplier *
                         AcousticsRuntimeVolume->OverrideDesignParams.DecayTimeMultiplier),
                        FAcousticsDesignParams::DecayTimeMultiplierMin,
                        FAcousticsDesignParams::DecayTimeMultiplierMax);
                    NewDesignParams.OutdoornessAdjustment = FMath::Clamp(
                        (NewDesignParams.OutdoornessAdjustment +
                         AcousticsRuntimeVolume->OverrideDesignParams.OutdoornessAdjustment),
                        FAcousticsDesignParams::OutdoornessAdjustmentMin,
                        FAcousticsDesignParams::OutdoornessAdjustmentMax);
                    NewDesignParams.WetRatioDistanceWarp = FMath::Clamp(
                        (NewDesignParams.WetRatioDistanceWarp *
                         AcousticsRuntimeVolume->OverrideDesignParams.WetRatioDistanceWarp),
                        FAcousticsDesignParams::WetRatioDistanceWarpMin,
                        FAcousticsDesignParams::WetRatioDistanceWarpMax);
                    NewDesignParams.TransmissionDb = FMath::Clamp(
                        (NewDesignParams.TransmissionDb + AcousticsRuntimeVolume->OverrideDesignParams.TransmissionDb),
                        FAcousticsDesignParams::TransmissionDbMin,
                        FAcousticsDesignParams::TransmissionDbMax);
                    NewDesignParams.ApplyDynamicOpenings |=
                        AcousticsRuntimeVolume->OverrideDesignParams.ApplyDynamicOpenings;
                }
            }
            CurrentDesignParams = NewDesignParams;
        }
    }
}

void UAcousticsAudioComponent::OnUnregister()
{
#if !UE_BUILD_SHIPPING
    if (m_Acoustics)
    {
        // Tell debug renderer this source is going away so stop rendering it
        m_Acoustics->UpdateSourceDebugInfo(
            GetAkGameObjectID(),
            ShowAcousticParameters || CVarAcousticDebug.GetValueOnGameThread() > 0,
            AkAudioEvent ? AkAudioEvent->GetFName() : "",
            true);
    }
#endif

    Super::OnUnregister();
}

void UAcousticsAudioComponent::TickComponent(
    float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Do not continue to querying acoustics if:
    //    - Acoustics module isn't available
    //    - We're not in game mode
    //    - The source isn't playing, and we're not displaying debug data
    if (!m_Acoustics || !GetWorld()->IsGameWorld() ||
        (!HasActiveEvents() && !(ShowAcousticParameters || CVarAcousticDebug.GetValueOnGameThread() > 0)))
    {
        return;
    }

    auto* AudioDevice = FAkAudioDevice::Get();

    // Can't do anything if AudioDevice isn't initialized
    if (AudioDevice == nullptr)
    {
        return;
    }

    // All checks passed -- query triton for this source
    FVector sourceLocation = this->GetComponentLocation();
    AkGameObjectID sourceID = GetAkGameObjectID();

    // Loop through all listeners and update parameters for each one

    TritonWwiseParams wwiseParams;
    UAkComponent* listener = AudioDevice->GetSpatialAudioListener();
    const auto listenerPosition = listener->GetOwner()->GetActorLocation();

    wwiseParams.Design = {CurrentDesignParams.OcclusionMultiplier,
                          CurrentDesignParams.WetnessAdjustment,
                          CurrentDesignParams.DecayTimeMultiplier,
                          CurrentDesignParams.OutdoornessAdjustment,
                          CurrentDesignParams.TransmissionDb,
                          CurrentDesignParams.WetRatioDistanceWarp};

    TritonDynamicOpeningInfo openingInfo = {};
    TritonDynamicOpeningInfo* openingPtr = CurrentDesignParams.ApplyDynamicOpenings ? &openingInfo : nullptr;

    bool acousticsSuccess =
        m_Acoustics->UpdateWwiseParameters(sourceID, sourceLocation, listenerPosition, wwiseParams, openingPtr);

#if !UE_BUILD_SHIPPING
    // Note: This call must be after acoustics computation as this information
    // is combined with internal acoustics debug information from prior call
    m_Acoustics->UpdateSourceDebugInfo(
        sourceID,
        this->ShowAcousticParameters || CVarAcousticDebug.GetValueOnGameThread() > 0,
        AkAudioEvent ? AkAudioEvent->GetFName() : "",
        false);
#endif
    // If query fails, don't update Wwise so that momentary failures
    // like listener head poking into a wall don't cause glitches
    if (!acousticsSuccess)
    {
        return;
    }

    const auto& ac = *m_Acoustics;
    SetWwiseDryPath(AudioDevice, listener, sourceLocation, listenerPosition, ac, wwiseParams);
    ComputeReverbSends(wwiseParams);

    //
    // Set RTPC on game object for opening filtering based on which opening sound went through
    //
    // If query fails for some reason, openingID might be returned unmodified as 0 (nullptr).
    // Or query succeeds, but either opening processing fails, or there is no opening
    // that the sound goes through - in both cases Triton sets all bits on openingID.
    if (!CurrentDesignParams.ApplyDynamicOpenings)
    {
        // If user dynamically switches off opening processing on a source,
        // make sure we clear out filtering
        SetOpeningFilteringRTPC(AudioDevice, 0.0f);
    }
    else
    {
        // If processing fails, we leave the last filtering value alone,
        // hoping that the failure was momentary.
        // Otherwise, we update.
        if (openingInfo.DidProcessingSucceed)
        {
            // If there is no opening on path, clear filtering
            if (!openingInfo.DidGoThroughOpening)
            {
                SetOpeningFilteringRTPC(AudioDevice, 0.0f);
            }
            // Otherwise, post the updated filtering value of opening on this source
            else
            {
                auto* opening = reinterpret_cast<UAcousticsDynamicOpening*>(openingInfo.OpeningID);
                // Protect against some opening disappearing without correctly de-registering
                // itself with acoustics engine
                if (IsValid(opening))
                {
                    const auto filtering = FMath::Clamp(100.0f * opening->Filtering, 0.0f, 100.0f);
                    SetOpeningFilteringRTPC(AudioDevice, filtering);
                }
            }
        }
    }
}

void UAcousticsAudioComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
    // Every time the acoustics audio component moves, check if it is inside a
    // volume and apply the acoustics design params overrides accordingly.
    if (GetWorld()->IsGameWorld())
    {
        ApplyAcousticsDesignParamsOverrides();
    }
}

bool UAcousticsAudioComponent::SetOpeningFilteringRTPC(FAkAudioDevice* AudioDevice, float filtering)
{
    // Optimize and early-exit if RTPC value is the same as already applied
    // This is especially important for sources not using dynamic openings so
    // they don't generate continuous RTPC traffic.
    if (filtering == LastFiltering)
    {
        return true;
    }

    auto* myOwningActor = GetOwner();
    if (!myOwningActor)
    {
        return false;
    }

    const float changeDuration = 0.01f;
    // We cache new value regardless of call success because
    // we don't want to spam Wwise regardless of cause of API failure
    LastFiltering = filtering;
    return AK_Success ==
           AudioDevice->SetRTPCValue(TEXT("AcousticsOpeningFiltering"), filtering, changeDuration, myOwningActor);
}

void UAcousticsAudioComponent::SetWwiseDryPath(
    FAkAudioDevice* AudioDevice, UAkComponent* listener, const FVector sourceLocation, const FVector listenerPosition,
    const IAcoustics& acoustics, const TritonWwiseParams& wwiseParams)
{
    TArray<FTransform> transformsArray;
    TArray<float> wwiseObsValues;
    TArray<float> wwiseOccValues;

    // MICHEM: I removed the transmitted path...

    // DIFFRACTED SHORTEST PATH
    // This is rendered coming as diffracted and attenuated around obstructions
    {
        float shortestDist = acoustics.TritonDelayToUnrealDistance(wwiseParams.TritonParams.DirectDelay);
        const float az = wwiseParams.TritonParams.DirectAzimuth;
        const float el = wwiseParams.TritonParams.DirectElevation;
        FVector portalDir = acoustics.TritonSphericalToUnrealCartesian(az, el);

        // Place portalled source at a distance equal to shortest path in portal direction.
        // This ensures Wwise will apply distance attenuation based on the "unfolded"
        // path length from source to listener that goes around obstructions
        FVector shortestPathSourcePos = listenerPosition + (portalDir * shortestDist);
        auto shortestPathTransform = FTransform(GetComponentRotation(), shortestPathSourcePos);

        // Arrival path uses the Project Acoustics-reported occlusion value.
        // Obstruction is turned off for the arrival path
        transformsArray.Add(shortestPathTransform);
        wwiseObsValues.Add(0);

        // Occlusion design is applied outside the mixer plugin because this value
        // will pass through Wwise's occlusion and distance attenuation design curves,
        // with final designed dry gain value available inside plugin.
        const float DesignedDirect = wwiseParams.TritonParams.DirectLoudnessDB * wwiseParams.Design.OcclusionMultiplier;
        auto occlusionValue = FMath::Clamp(DesignedDirect / -100.0f, 0.0f, 1.0f);
        wwiseOccValues.Add(occlusionValue);
    }

    // Configure multiple positions and their corresponding occlusion/obstruction values.
    AudioDevice->SetMultiplePositions(this, transformsArray, AkMultiPositionType::SingleSource);
    AudioDevice->SetMultipleObstructionAndOcclusion(this, listener, wwiseObsValues, wwiseOccValues);
}
const TArray<float> c_ReverbDecayTimes = { 0.5f, 1.0f, 1.5f, 3.0f };
const FString c_AuxBusNames[6] = { TEXT("Verb_X_Minus"),
                                TEXT("Verb_X_Plus"),
                                TEXT("Verb_Y_Minus"),
                                TEXT("Verb_Y_Plus"),
                                TEXT("Verb_Z_Minus"),
                                TEXT("Verb_Z_Plus") };
const int c_AuxBusCount = 6;

const FString c_ExtendedAuxBusNames[] = { TEXT("XM_Short"),
                                TEXT("XM_Med"),
                                TEXT("XM_Long"),
                                TEXT("XM_XLong"),
                                TEXT("XP_Short"),
                                TEXT("XP_Med"),
                                TEXT("XP_Long"),
                                TEXT("XP_XLong"),
                                TEXT("YM_Short"),
                                TEXT("YM_Med"),
                                TEXT("YM_Long"),
                                TEXT("YM_XLong"),
                                TEXT("YP_Short"),
                                TEXT("YP_Med"),
                                TEXT("YP_Long"),
                                TEXT("YP_XLong"),
                                TEXT("ZM_Short"),
                                TEXT("ZM_Med"),
                                TEXT("ZM_Long"),
                                TEXT("ZM_XLong"),
                                TEXT("ZP_Short"),
                                TEXT("ZP_Med"),
                                TEXT("ZP_Long"),
                                TEXT("ZP_XLong")};
const int c_ExtendedAuxBusCount = 18;

float GetReverbFromIndex(int index, const TritonAcousticParameters& T, float dbSPL)
{
    float tritonVol = -96;
    if (index == 0)
    {
        // Want Minus X
        tritonVol = T.ReflLoudnessDB_Channel_2;
    }
    else if (index == 1)
    {
        // Want Plus X
        tritonVol = T.ReflLoudnessDB_Channel_1;
    }
    else if (index == 2)
    {
        // Want Minus Y
        tritonVol = T.ReflLoudnessDB_Channel_3;
    }
    else if (index == 3)
    {
        // Want Plus Y
        tritonVol = T.ReflLoudnessDB_Channel_4;
    }
    else if (index == 4)
    {
        // Want Minus Z
        tritonVol = T.ReflLoudnessDB_Channel_0;
    }
    else if (index == 5)
    {
        // Want Plus Z
        tritonVol = T.ReflLoudnessDB_Channel_5;
    }
    // Now must map values from -96:24 to 0.0f:16.0f, per AK API Contract
    // This means AK is expecting amplitude. Convert Reflection db to amplitude
    return FMath::Clamp(FMath::Pow(10.0f, (tritonVol + dbSPL) / 20.0f), 0.0f, 16.0f);
}

bool UAcousticsAudioComponent::ComputeReverbSends(TritonWwiseParams& emitterParams)
{
    auto akd = FAkAudioDevice::Get();
    if (akd == nullptr)
    {
        return false;
    }

    // Get SPL for this source
    // Subtract from 100. 100 is the loudest possible source, which would equate to the loudest possible reverb (wwise volume at 0)
    // Everything else is attenuation loss according to wwise
    float dbSPL = (m_SecondarySource->SoundSourceLoudness - 100.0f);

    const auto T = emitterParams.TritonParams;
    AkAuxSendValue tmpAuxSend;
    TArray<AkAuxSendValue> auxSends;
    for (int i = 0; i < c_AuxBusCount; i++)
    {
        const auto reverbAmplitude = GetReverbFromIndex(i, T, dbSPL);
        for (int j = 0; j < c_ReverbDecayTimes.Num(); j++)
        {
            tmpAuxSend.auxBusID = akd->GetIDFromString(c_ExtendedAuxBusNames[i*c_ReverbDecayTimes.Num() + j]);
            tmpAuxSend.listenerID = akd->GetSpatialAudioListener()->GetAkGameObjectID();
            tmpAuxSend.fControlValue = CalculateRT60Sends(T.ReverbTime, reverbAmplitude, j);
            auxSends.Add(tmpAuxSend);
        }
    }
    
    AKRESULT success = akd->SetAuxSends(this, auxSends);
    return success == AKRESULT::AK_Success;
}

// Note: The busIndex param directly indexes into c_ReverbDecayTimes.
float UAcousticsAudioComponent::CalculateRT60Sends(float targetDecayTime, float wetnessAmplitude, int busIndex)
{
    int upperDecayIndex =
        c_ReverbDecayTimes.IndexOfByPredicate([=](float filterDecayTime) -> bool { return filterDecayTime > targetDecayTime; });
    
    // Target RT60 is larger than the largets we support. If we are currently calculating for the longest bus, return full reverb power
    if (upperDecayIndex == INDEX_NONE && busIndex == c_ReverbDecayTimes.Num() - 1)
    {
        return wetnessAmplitude;
    }   
    else if (upperDecayIndex == 0 && busIndex == 0)
    {
        // target decay time is smaller than our shortest reverb. Use the shortest reverb exclusively.
        return wetnessAmplitude;
    }
    else if (busIndex == upperDecayIndex || busIndex == upperDecayIndex - 1)
    {
        // Blending between two reverbs and we are querying for one of those
        float shorterDecayWeight = ComputeDecayTimeInterpolationWeight(
            c_ReverbDecayTimes[upperDecayIndex - 1], c_ReverbDecayTimes[upperDecayIndex], targetDecayTime);
        if (busIndex == upperDecayIndex - 1)
        {
            return shorterDecayWeight * wetnessAmplitude;
        }
        else
        {
            return (1.0f - shorterDecayWeight) * wetnessAmplitude;
        }
    }
    return 0;
}

// TODO: What was this ever for?
#define MATCHING_TIME_PERCENTAGE 1.0f;

float UAcousticsAudioComponent::ComputeDecayTimeInterpolationWeight(
    float ShortDecayTime, float LongDecayTime, float TargetDecayTime)
{
    check(ShortDecayTime < LongDecayTime);
    float MatchingTime = TargetDecayTime * MATCHING_TIME_PERCENTAGE;
    float TargetReverbAmp = FMath::Pow(10.0f, -3.0f * MatchingTime / TargetDecayTime);
    float ShortReverbAmp = FMath::Pow(10.0f, -3.0f * MatchingTime / ShortDecayTime);
    float LongReverbAmp = FMath::Pow(10.0f, -3.0f * MatchingTime / LongDecayTime);
    return (TargetReverbAmp - LongReverbAmp) / (ShortReverbAmp - LongReverbAmp);
}