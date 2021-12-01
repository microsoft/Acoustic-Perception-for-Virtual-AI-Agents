// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AcousticsSpace.h"
#include "IAcoustics.h"
#include "AkInclude.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include <Classes/GameFramework/HUD.h>
#include <Classes/GameFramework/PlayerController.h>

const FString c_ProjectAcousticsBusName = TEXT("Project Acoustics Bus");
const AkUInt32 c_MSCompanyID = 275;
const AkUInt32 c_MSAcousticsPluginID = 1;

// Console commands for toggling debug info
static TAutoConsoleVariable<int32>
    CVarAcousticsDrawVoxels(TEXT("PA.DrawVoxels"), 0, TEXT("Show Project Acoustics voxels?"));
static TAutoConsoleVariable<int32>
    CVarAcousticsDrawProbes(TEXT("PA.DrawProbes"), 0, TEXT("Show Project Acoustics probes?"));
static TAutoConsoleVariable<int32>
    CVarAcousticsDrawDistances(TEXT("PA.DrawDistances"), 0, TEXT("Show Project Acoustics distance data?"));
static TAutoConsoleVariable<int32>
    CVarAcousticsShowStats(TEXT("PA.ShowStats"), 0, TEXT("Show Project Acoustics statistics?"));

AAcousticsSpace::AAcousticsSpace(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    // The source component is set to tick TG_DuringPhysics.
    // We set the acoustic space's tick here to be
    // sequenced after all source ticks using TG_PostPhysics.
    // This avoids a potential extra frame delay in refreshing the
    // acoustic parameters. This can have audible issues,
    // especially for new sounds spawned in an occluded position
    // on this frame
    PrimaryActorTick.TickGroup = TG_PostPhysics;

    // Main parameters
    TileSize = FVector(5000, 5000, 5000);
    AutoStream = true;
    UpdateDistances = false;
    CacheScale = 1.0f;

    m_Acoustics = nullptr;

    // Design controls
    auto d = UserDesign::Default();
    OcclusionMultiplier = d.OcclusionMultiplier;
    WetnessAdjustment = d.WetnessAdjustment;
    DecayTimeMultiplier = d.DecayTimeMultiplier;
    OutdoornessAdjustment = d.OutdoornessAdjustment;
    WetRatioDistanceWarp = d.DRRDistanceWarp;
    TransmissionDb = d.TransmissionDb;

    // Debug controls
    AcousticsEnabled = true;
    DrawStats = false;
    DrawVoxels = false;
    DrawProbes = false;
    DrawDistances = false;
}

void AAcousticsSpace::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickEnabled(true);
    if (IAcoustics::IsAvailable())
    {
        // cache module instance
        m_Acoustics = &(IAcoustics::Get());

#if !UE_BUILD_SHIPPING
        // Update with current enabled state
        m_Acoustics->SetEnabled(AcousticsEnabled);
#endif

        auto success = LoadAcousticsData(AcousticsData);

        if (success && AutoStream)
        {
            // Stream in the first tile if AutoLoad is enabled
            auto listenerPosition = GetListenerPosition();
            m_Acoustics->UpdateLoadedRegion(listenerPosition, TileSize, true, true, true);
        }
    }

#if !UE_BUILD_SHIPPING
    // On startup, tell our HUD to allow draw debug overlays
    // and start calling our PostRenderFor()
    auto fpc = GetWorld()->GetFirstPlayerController();
    if (fpc)
    {
        auto HUD = fpc->GetHUD();
        if (HUD)
        {
            HUD->bShowOverlays = true;
            HUD->AddPostRenderedActor(this);
        }
    }
#endif //! UE_BUILD_SHIPPING
}

// Get location of first listener
FVector AAcousticsSpace::GetListenerPosition()
{
    if (auto* AudioDevice = FAkAudioDevice::Get())
    {
        return AudioDevice->GetSpatialAudioListener()->GetOwner()->GetActorLocation();
    }
    else
    {
        auto world = GetWorld();
        auto cameraManager = world->GetFirstPlayerController()->PlayerCameraManager;
        return cameraManager->GetCameraLocation();
    }
}

// Note: This function will be called after all source component ticks.
void AAcousticsSpace::Tick(float deltaSeconds)
{
    Super::Tick(deltaSeconds);

    if (!m_Acoustics)
    {
        return;
    }

    // Update global design tweaks
    {
        UserDesign globalParams = {OcclusionMultiplier,
                                   WetnessAdjustment,
                                   DecayTimeMultiplier,
                                   OutdoornessAdjustment,
                                   TransmissionDb,
                                   WetRatioDistanceWarp};

        m_Acoustics->SetGlobalDesign(globalParams);
    }

    // Update things dependent only on listener
    if (GetWorld()->IsGameWorld())
    {
        auto listenerPosition = GetListenerPosition();

        // Update streaming
        if (AutoStream)
        {
            // TODO: MICHEM: Changed to blocking load to try to prevent heap issues
            m_Acoustics->UpdateLoadedRegion(listenerPosition, TileSize, false, true, true);
        }

        // If there are active emitters in the scene, they will
        // update outdoorness each frame automatically. But if there are
        // no active emitters this frame, we hand-crank outdoorness.
        m_Acoustics->UpdateOutdoorness(listenerPosition);

        // Update distances
        if (UpdateDistances)
        {
            m_Acoustics->UpdateDistances(listenerPosition);
        }
    }

    // MICHEM: Not using Mixer plugin. Not needed
    // Update the mixer plugin with the latest Triton parameters
    //const auto& paramsCache = IAcoustics::Get().GetCachedWwiseParameters();
    //m_PluginData.Empty(paramsCache.Num());
    //for (const auto& pair : paramsCache)
    //{
    //    m_PluginData.Add(pair.Value);
    //}

    /*FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();

    AudioDevice->SendPluginCustomGameData(
        AudioDevice->GetIDFromString(c_ProjectAcousticsBusName),
        AK_INVALID_GAME_OBJECT,
        AkPluginTypeMixer,
        c_MSCompanyID,
        c_MSAcousticsPluginID,
        m_PluginData.GetData(),
        m_PluginData.Num() * sizeof(TritonWwiseParams));*/

    // Inform processing for this frame is complete, updates internal per-frame state
    m_Acoustics->PostTick();
}

void AAcousticsSpace::BeginDestroy()
{
    Super::BeginDestroy();
    if (m_Acoustics)
    {
        m_Acoustics->UnloadAceFile();
    }
}

void AAcousticsSpace::ForceLoadTile(FVector centerPosition, bool unloadProbesOutsideTile, bool )
{
    if (!m_Acoustics)
    {
        return;
    }

    m_Acoustics->UpdateLoadedRegion(centerPosition, TileSize, true, unloadProbesOutsideTile, true);
}

bool AAcousticsSpace::LoadAcousticsData(UAcousticsData* newData)
{
    AcousticsData = newData;
    if (newData == nullptr)
    {
        if (m_Acoustics)
        {
            m_Acoustics->UnloadAceFile();
        }
        return true;
    }
    auto filePath = newData->AceFilePath;
    return LoadAceFile(filePath);
}

bool AAcousticsSpace::LoadAceFile(FString filePath)
{
    if (!m_Acoustics)
    {
        return false;
    }

    auto success = m_Acoustics->LoadAceFile(filePath, CacheScale);
    if (success)
    {
        if (AutoStream)
        {
            auto listenerPosition = GetListenerPosition();
            m_Acoustics->UpdateLoadedRegion(listenerPosition, TileSize, true, true, true);
        }
    }
    else
    {
        UE_LOG(LogAcousticsRuntime, Error, TEXT("Failed to load ACE file [%s]"), *filePath);
        return false;
    }

    return true;
}

bool AAcousticsSpace::QueryDistance(const FVector lookDirection, float& distance)
{
    if (!m_Acoustics)
    {
        distance = 0;
        return false;
    }

    return m_Acoustics->QueryDistance(lookDirection, distance);
}

bool AAcousticsSpace::GetOutdoorness(float& outdoorness)
{
    if (!m_Acoustics)
    {
        outdoorness = 0;
        return false;
    }

    outdoorness = m_Acoustics->GetOutdoorness();
    return true;
}

void AAcousticsSpace::SetAcousticsEnabled(bool isEnabled)
{
    if (m_Acoustics)
    {
        AcousticsEnabled = isEnabled;
#if !UE_BUILD_SHIPPING
        m_Acoustics->SetEnabled(AcousticsEnabled);
#endif
    }
}

#if WITH_EDITOR
// React to changes in properties that are not handled in Tick()
void AAcousticsSpace::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
    Super::PostEditChangeProperty(e);
    auto world = GetWorld();
    if (world && world->IsGameWorld())
    {
        // If ace file name updated, load new file
        FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AAcousticsSpace, AcousticsData))
        {
            LoadAcousticsData(AcousticsData);
        }

#if !UE_BUILD_SHIPPING
        // React to acoustic effects being toggled
        PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
        if (PropertyName == GET_MEMBER_NAME_CHECKED(AAcousticsSpace, AcousticsEnabled))
        {
            m_Acoustics->SetEnabled(AcousticsEnabled);
        }
#endif
    }
}
#endif

#if !UE_BUILD_SHIPPING
void AAcousticsSpace::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
    if (!m_Acoustics)
    {
        return;
    }

    float CameraFOV = 90.0f;
    if (PC && PC->PlayerCameraManager)
    {
        CameraFOV = PC->PlayerCameraManager->GetFOVAngle();
    }

    m_Acoustics->SetVoxelVisibleDistance(VoxelsVisibleDistance);

    m_Acoustics->DebugRender(
        GetWorld(),
        Canvas,
        CameraPosition,
        CameraDir,
        CameraFOV,
        DrawStats || CVarAcousticsShowStats.GetValueOnGameThread() > 0,
        DrawVoxels || CVarAcousticsDrawVoxels.GetValueOnGameThread() > 0,
        DrawProbes || CVarAcousticsDrawProbes.GetValueOnGameThread() > 0,
        (UpdateDistances & (DrawDistances || CVarAcousticsDrawDistances.GetValueOnGameThread() > 0)));
}
#endif // UE_BUILD_SHIPPING
