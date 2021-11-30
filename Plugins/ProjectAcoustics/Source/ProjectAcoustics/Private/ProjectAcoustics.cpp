// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
// The Project Acoustics module

#include "ProjectAcoustics.h"
#include "IAcoustics.h"
#include "AcousticsDebugRender.h"
#include "MathUtils.h"

using namespace TritonRuntime;

#define LOCTEXT_NAMESPACE "FProjectAcousticsModule"
DEFINE_LOG_CATEGORY(LogAcousticsRuntime)

DEFINE_STAT(STAT_Acoustics_UpdateWwiseParams);
DEFINE_STAT(STAT_Acoustics_Query);
DEFINE_STAT(STAT_Acoustics_QueryOutdoorness);
DEFINE_STAT(STAT_Acoustics_LoadRegion);
DEFINE_STAT(STAT_Acoustics_LoadAce);
DEFINE_STAT(STAT_Acoustics_ClearAce);

// Speed of sound in cm/sec
constexpr int c_SpeedOfSoundCm = 34000;

// Safety margin for ACE streaming loads.
// When player gets to within this fraction of the loaded region's border,
// a new region is loaded, centered at the player.
// 0 is extremely safe but lots of I/O, 1 is no safety.
float c_AceTileLoadMargin = 0.8f;
// Console control of changing the load margin
static FAutoConsoleVariableRef CVarAcousticsAceTileLoadMargin(
    TEXT("PA.AceTileLoadMargin"), c_AceTileLoadMargin,
    TEXT("Safety margin for ACE streaming loads.\n")
        TEXT("When player gets to within this fraction of the loaded region's border,\n")
            TEXT("a new region is loaded, centered at the player.\n")
                TEXT("0 is extremely safe but lots of I/O, 1 is no safety.\n"),
    ECVF_Default);

// Computed outdoorness is 0 only if player is completely enclosed
// and 1 only when player is standing on a flat plane with no other geometry.
// These constants bring the range closer to practically observed values.
// Tune as necessary.
constexpr float c_OutdoornessIndoors = 0.02f;
constexpr float c_OutdoornessOutdoors = 1.0f;

// Triton's debug interface lets you query things like the voxel display and probe stats
// This is very helpful during development, but shouldn't be used when the game is shipped
#if !UE_BUILD_SHIPPING
constexpr bool c_UseTritonDebugInterface = true;
#else
constexpr bool c_UseTritonDebugInterface = false;
#endif

FProjectAcousticsModule::FProjectAcousticsModule()
    : m_Triton(nullptr)
    , m_AceFileLoaded(false)
    , m_LastLoadCenterPosition(0, 0, 0)
    , m_LastLoadTileSize(0, 0, 0)
    , m_IsOutdoornessStale(true)
    , m_CachedOutdoorness(0)
    , m_GlobalDesign(UserDesign::Default())
{
#if !UE_BUILD_SHIPPING
    m_IsEnabled = true;
#endif
}

void FProjectAcousticsModule::StartupModule()
{
    m_TritonMemHook = TUniquePtr<FTritonMemHook>(new FTritonMemHook());
    m_TritonLogHook = TUniquePtr<FTritonLogHook>(new FTritonLogHook());
    auto initSuccess = TritonAcoustics::Init(m_TritonMemHook.Get(), m_TritonLogHook.Get());
    if (!initSuccess)
    {
        UE_LOG(LogAcousticsRuntime, Error, TEXT("Project Acoustics failed to initialize!"));
        return;
    }

    m_Triton = c_UseTritonDebugInterface ? TritonAcousticsDebug::CreateInstance() : TritonAcoustics::CreateInstance();

    if (!m_Triton)
    {
        UE_LOG(LogAcousticsRuntime, Error, TEXT("Project Acoustics failed to create instance!"));
        return;
    }

#if !UE_BUILD_SHIPPING
    // setup debug rendering for ourself
    m_DebugRenderer.Reset(new FProjectAcousticsDebugRender(this));
#endif
}

void FProjectAcousticsModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
    if (m_Triton)
    {
        TritonAcoustics::DestroyInstance(m_Triton);
        TritonAcoustics::TearDown();
        m_Triton = nullptr;

#if !UE_BUILD_SHIPPING
        m_DebugRenderer.Reset();
#endif
    }
}

float FProjectAcousticsModule::TritonDelayToUnrealDistance(float delay) const
{
    return delay * c_SpeedOfSoundCm;
}

FVector FProjectAcousticsModule::TritonSphericalToUnrealCartesian(float azimuth, float elevation) const
{
    constexpr auto c_DegToRadian = PI / 180.0f;

    azimuth *= c_DegToRadian;
    elevation *= c_DegToRadian;

    auto z = FMath::Cos(elevation);
    auto horiz = FMath::Sin(elevation);
    auto x = horiz * FMath::Cos(azimuth);
    auto y = horiz * FMath::Sin(azimuth);

    // Reverse direction to go from emitter-to-listener to listener-to-emitter
    // Then transform to game's coordinate system
    return TritonDirectionToUnreal(FVector{-x, -y, -z});
}

bool FProjectAcousticsModule::LoadAceFile(const FString& filePath, const float cacheScale)
{
    if (!m_Triton)
    {
        return false;
    }

    UnloadAceFile();

    auto fullFilePath = FPaths::ProjectDir() + filePath;
    {
        SCOPE_CYCLE_COUNTER(STAT_Acoustics_LoadAce);
        // Load the ACE file
        m_TritonIOHook = TUniquePtr<FTritonUnrealIOHook>(new FTritonUnrealIOHook());
        if (!m_TritonIOHook->OpenForRead(TCHAR_TO_ANSI(*fullFilePath)))
        {
            m_TritonIOHook.Reset();

            UE_LOG(LogAcousticsRuntime, Error, TEXT("Failed to open ACE file for reading: [%s]"), *fullFilePath);
            return false;
        }

        m_TritonTaskHook = TUniquePtr<FTritonAsyncTaskHook>(new FTritonAsyncTaskHook());
        if (!m_Triton->InitLoad(m_TritonIOHook.Get(), m_TritonTaskHook.Get(), cacheScale))
        {
            UE_LOG(LogAcousticsRuntime, Error, TEXT("Failed to load ACE file: [%s]"), *fullFilePath);
            return false;
        }
    }

    m_AceFileLoaded = true;

#if !UE_BUILD_SHIPPING
    m_DebugRenderer->SetLoadedFilename(filePath);
#endif

    return true;
}

void FProjectAcousticsModule::UnloadAceFile()
{
    if (!m_Triton)
    {
        return;
    }

    if (m_AceFileLoaded)
    {
        SCOPE_CYCLE_COUNTER(STAT_Acoustics_ClearAce);
        m_Triton->Clear();
        m_AceFileLoaded = false;
    }

    m_TritonIOHook.Reset();
}

#if !UE_BUILD_SHIPPING
static void UnrealCartesianToTritonSpherical(const FVector& arrivalDir, float& azimuth, float& elevation)
{
    auto v = UnrealDirectionToTriton(arrivalDir);
    v.Normalize();

    elevation = FMath::Acos(v.Z);
    v.Z = 0;
    azimuth = FMath::Atan2(v.Y, v.X);

    const float toDegrees = 180.0f / PI;
    elevation *= toDegrees;
    azimuth *= toDegrees;
}

static TritonAcousticParameters MakeFreefieldParameters(const FVector& sourceLocation, const FVector& listenerLocation)
{
    auto arrivalDir = listenerLocation - sourceLocation;
    auto LOSDist = arrivalDir.Size();
    auto delay = FMath::Max(100.0f, LOSDist) / c_SpeedOfSoundCm;
    float azimuth = 0, elevation = 0;
    UnrealCartesianToTritonSpherical(arrivalDir, azimuth, elevation);
    const float silenceDb = -120.0f;
    const float zeroDecayTime = 0;

    return TritonAcousticParameters{delay,
                                    0,
                                    azimuth,
                                    elevation,
                                    0,
                                    silenceDb,
                                    silenceDb,
                                    silenceDb,
                                    silenceDb,
                                    silenceDb,
                                    silenceDb,
                                    silenceDb,
                                    zeroDecayTime,
                                    zeroDecayTime};
}
#endif

bool FProjectAcousticsModule::AddDynamicOpening(
    class UAcousticsDynamicOpening* opening, const FVector& center, const FVector& normal,
    const TArray<FVector>& verticesIn)
{
    if (!m_Triton || verticesIn.Num() == 0)
    {
        return false;
    }

    TArray<Triton::Vec3f> vertices;
    for (auto& v : verticesIn)
    {
        vertices.Add(ToTritonVector(v));
    }

    return m_Triton->AddDynamicOpening(
        reinterpret_cast<uint64_t>(opening),
        ToTritonVector(center),
        ToTritonVector(normal),
        vertices.Num(),
        &vertices[0]);
}

bool FProjectAcousticsModule::RemoveDynamicOpening(class UAcousticsDynamicOpening* opening)
{
    if (!m_Triton)
    {
        return false;
    }

    return m_Triton->RemoveDynamicOpening(reinterpret_cast<uint64_t>(opening));
}

bool FProjectAcousticsModule::UpdateDynamicOpening(
    class UAcousticsDynamicOpening* opening, float dryAttenuationDb, float wetAttenuationDb)
{
    if (!m_Triton)
    {
        return false;
    }

    return m_Triton->UpdateDynamicOpening(reinterpret_cast<uint64_t>(opening), dryAttenuationDb, wetAttenuationDb);
}

bool FProjectAcousticsModule::SetGlobalDesign(const UserDesign& params)
{
    m_GlobalDesign = params;
    return true;
}

bool FProjectAcousticsModule::UpdateWwiseParameters(
    const uint64_t akSourceObjectId, const FVector& sourceLocation, const FVector& listenerLocation,
    TritonWwiseParams& wwiseParams, TritonDynamicOpeningInfo* outOpeningInfo)
{
    SCOPE_CYCLE_COUNTER(STAT_Acoustics_UpdateWwiseParams);

    if (!m_Triton)
    {
        return false;
    }

    // Validate arguments
    if (!m_AceFileLoaded)
    {
        return false;
    }

    // First emitter that calls to update its acoustic parameters
    // in a frame will do work to update the stale outdoorness value.
    UpdateOutdoorness(listenerLocation);

    // Get acoustic parameters from Triton. Pass failure on to caller, caller should re-use previous acoustic parameters
    TritonAcousticParameters acousticParams;

#if !UE_BUILD_SHIPPING
    TritonRuntime::QueryDebugInfo queryDebugInfo;
    const bool querySuccess =
        GetAcousticParameters(sourceLocation, listenerLocation, acousticParams, outOpeningInfo, &queryDebugInfo);
    if (!querySuccess)
    {
        // Even if query fails, we want to catch that debug information before exiting this function
        m_DebugRenderer->UpdateSourceAcoustics(
            akSourceObjectId, sourceLocation, listenerLocation, querySuccess, wwiseParams, queryDebugInfo);

        return false;
    }
#else
    const bool querySuccess = GetAcousticParameters(sourceLocation, listenerLocation, acousticParams, outOpeningInfo);
    if (!querySuccess)
    {
        return false;
    }
#endif // !UE_BUILD_SHIPPING

    // Update DirectLoudness to be compensated with shortest path distance attenuation
    // instead of line-of-sight distance.
    // [TASK 20188902] Update to return directly from QueryAcoustics() and delete this code.
    {
        auto shortestPathDistance = FMath::Max(100.0f, TritonDelayToUnrealDistance(acousticParams.DirectDelay));
        auto LOSDistance = FMath::Max(100.0f, (sourceLocation - listenerLocation).Size());
        auto directLoudnessAmpl = DbToAmplitude(acousticParams.DirectLoudnessDB);
        auto directLoudnessGeodesicDb = AmplitudeToDb(shortestPathDistance / LOSDistance * directLoudnessAmpl);
        acousticParams.DirectLoudnessDB = directLoudnessGeodesicDb;
    }

    // Caller passes in design adjustments for this emitter,
    // update them with global adjustments
    UserDesign::Combine(wwiseParams.Design, m_GlobalDesign);

    // Set the remaining fields apart from design
    wwiseParams.ObjectId = akSourceObjectId;
    wwiseParams.TritonParams = acousticParams;
    // Outdoorness value is shared across all emitters since it depends only on
    // listener location (for now), fill in that shared value.
    wwiseParams.Outdoorness = m_CachedOutdoorness;

#if !UE_BUILD_SHIPPING
    // If acoustics is disabled, intercept parameters headed to DSP
    // and substitute "no acoustics" in all parameters - i.e. how it
    // would sound if there were no geometry in the scene. Note that
    // the system's internal logic such as doing queries, updating streaming
    // etc. still remain active. This is intentional since the intended use
    // case is for someone to do a quick A/B by toggling switch to hear difference
    // such as for debugging.
    if (!m_IsEnabled)
    {
        wwiseParams.TritonParams = MakeFreefieldParameters(sourceLocation, listenerLocation);
        wwiseParams.Outdoorness = 1;
    }

    // Catch debug information for this source
    m_DebugRenderer->UpdateSourceAcoustics(
        akSourceObjectId, sourceLocation, listenerLocation, querySuccess, wwiseParams, queryDebugInfo);
#endif

    CollectPluginData(wwiseParams);
    return true;
}

bool FProjectAcousticsModule::PostTick()
{
    if (!m_Triton)
    {
        return false;
    }

    m_WwiseParamsCache.Empty();
    m_IsOutdoornessStale = true;
    return true;
}

// Collects data across emitters to send to Wwise mixer plugin
void FProjectAcousticsModule::CollectPluginData(const TritonWwiseParams& params)
{
    // This means we're updating the acoustics for a game object multiple times
    // in a frame. That wastes computation.

    // check() will be triggered when a new component is spawn in BP,
    //	which will trigger an TickComponent in TG_LastDemotable tick group, which is after m_WwiseParamsCache is
    // cleared. 	When the next "regular" TickComponent comes in the next frame, the object params is already in the
    // m_WwiseParamsCache. check(!m_WwiseParamsCache.Contains(params.ObjectId)); m_WwiseParamsCache.Add(params.ObjectId,
    // params);

    TritonWwiseParams& newParams = m_WwiseParamsCache.FindOrAdd(params.ObjectId);
    newParams = params;
}

bool FProjectAcousticsModule::UpdateDistances(const FVector& listenerLocation)
{
    if (!m_Triton)
    {
        return false;
    }

    auto listener = ToTritonVector(UnrealPositionToTriton(listenerLocation));
    return m_Triton->UpdateDistancesForListener(listener);
}

bool FProjectAcousticsModule::QueryDistance(const FVector& lookDirection, float& outDistance)
{
    if (!m_Triton)
    {
        outDistance = 0;
        return false;
    }

    auto dir = ToTritonVector(UnrealDirectionToTriton(lookDirection));
    outDistance = m_Triton->QueryDistanceForListener(dir) * c_TritonToUnrealScale;
    return true;
}

bool FProjectAcousticsModule::UpdateOutdoorness(const FVector& listenerLocation)
{
    if (!m_Triton)
    {
        return false;
    }

    // This function will be called by each sound source in a frame.
    // Since outdoorness depends only on player location, we do work
    // only once per frame, regardless of whether query succeeds or fails.
    // In case of failure, we leave the old cached outdoorness value unmodified.
    if (m_IsOutdoornessStale)
    {
        auto listener = ToTritonVector(UnrealPositionToTriton(listenerLocation));
        bool success = false;
        {
            SCOPE_CYCLE_COUNTER(STAT_Acoustics_QueryOutdoorness);
            auto outdoorness = 0.0f;
            success = m_Triton->GetOutdoornessAtListener(listener, outdoorness);
            if (success)
            {
                const float NormalizedVal =
                    (outdoorness - c_OutdoornessIndoors) / (c_OutdoornessOutdoors - c_OutdoornessIndoors);
                m_CachedOutdoorness = FMath::Clamp(NormalizedVal, 0.0f, 1.0f);
            }
        }

        m_IsOutdoornessStale = false;
        return success;
    }

    return true;
}

inline float FProjectAcousticsModule::GetOutdoorness() const
{
    return m_CachedOutdoorness;
}

bool FProjectAcousticsModule::QueryAcoustics(const int sourceId, const FVector& sourceLocation, const FVector& listenerLocation, TritonAcousticParameters& outParams)
{
    TritonWwiseParams params;
    QueryDebugInfo qdi;
    bool retVal = GetAcousticParameters(sourceLocation, listenerLocation, outParams,nullptr, &qdi);
    params.TritonParams = outParams;
    // Even if query fails, we want to catch that debug information before exiting this function
    m_DebugRenderer->UpdateSourceAcoustics(
        sourceId, sourceLocation, listenerLocation, retVal, params, qdi);
    return retVal;
}

bool FProjectAcousticsModule::GetAcousticParameters(
    const FVector& sourceLocation, const FVector& listenerLocation, TritonAcousticParameters& params,
    TritonDynamicOpeningInfo* outOpeningInfo, TritonRuntime::QueryDebugInfo* outDebugInfo /* = nullptr */)
{
    auto source = ToTritonVector(UnrealPositionToTriton(sourceLocation));
    auto listener = ToTritonVector(UnrealPositionToTriton(listenerLocation));

    bool acousticParamsValid = false;
    {
        SCOPE_CYCLE_COUNTER(STAT_Acoustics_Query);
#if !UE_BUILD_SHIPPING
        acousticParamsValid =
            GetTritonDebugInstance()->QueryAcoustics(source, listener, params, outOpeningInfo, outDebugInfo);
#else
        acousticParamsValid = m_Triton->QueryAcoustics(source, listener, params, outOpeningInfo);
#endif
    }

    // Triton returns granular failure per parameters.
    // Here we enforce success only if all parameters can be successfully computed.
    if (!acousticParamsValid || (params.DirectLoudnessDB == TritonAcousticParameters::FailureCode ||
                                 params.DirectAzimuth == TritonAcousticParameters::FailureCode ||
                                 params.DirectElevation == TritonAcousticParameters::FailureCode ||
                                 params.ReflectionsLoudnessDB == TritonAcousticParameters::FailureCode ||
                                 params.EarlyDecayTime == TritonAcousticParameters::FailureCode ||
                                 params.ReverbTime == TritonAcousticParameters::FailureCode))
    {
        return false;
    }

    return true;
}

void FProjectAcousticsModule::UpdateLoadedRegion(
    const FVector& playerPosition, const FVector& tileSize, const bool forceUpdate, const bool unloadProbesOutsideTile,
    const bool blockOnCompletion)
{
    if (!m_Triton)
    {
        return;
    }

    const auto difference = (playerPosition - m_LastLoadCenterPosition).GetAbs();
    const auto loadThreshold = m_LastLoadTileSize * c_AceTileLoadMargin * 0.5f;
    bool shouldUpdate = forceUpdate || (difference.X > loadThreshold.X || difference.Y > loadThreshold.Y ||
                                        difference.Z > loadThreshold.Z);
    if (shouldUpdate)
    {
        int loadedProbes = 0;
        {
            SCOPE_CYCLE_COUNTER(STAT_Acoustics_LoadRegion);

            loadedProbes = m_Triton->LoadRegion(
                ToTritonVector(UnrealPositionToTriton(playerPosition)),
                ToTritonVector(UnrealPositionToTriton(tileSize).GetAbs()),
                unloadProbesOutsideTile,
                blockOnCompletion);
        }
        if (loadedProbes > 0)
        {
            m_LastLoadCenterPosition = playerPosition;
            // Tile Size must be all positive values, otherwise triton fails to load probes
            m_LastLoadTileSize = tileSize.GetAbs();
        }
    }
}

const TMap<uint64_t, TritonWwiseParams>& FProjectAcousticsModule::GetCachedWwiseParameters()
{
    return m_WwiseParamsCache;
}

#if !UE_BUILD_SHIPPING
TritonAcousticsDebug* FProjectAcousticsModule::GetTritonDebugInstance() const
{
    return static_cast<TritonAcousticsDebug*>(m_Triton);
}

void FProjectAcousticsModule::SetEnabled(bool isEnabled)
{
    m_IsEnabled = isEnabled;
}

// Sets flag for this source to render debug information (or not)
void FProjectAcousticsModule::UpdateSourceDebugInfo(
    uint64_t sourceID, bool shouldDraw, FName displayName, bool isLoudest, bool isConfused)
{
    if (!m_Triton)
    {
        return;
    }

    m_DebugRenderer->UpdateSourceDebugInfo(sourceID, shouldDraw, displayName, isLoudest, isConfused);
}

void FProjectAcousticsModule::UpdateConfidenceValues(
    FVector direction, float confidence)
{
    if (!m_Triton)
    {
        return;
    }

    m_DebugRenderer->UpdateConfidenceVector(direction, confidence);
}

void FProjectAcousticsModule::SetVoxelVisibleDistance(const float InVisibleDistance)
{
    if (m_DebugRenderer)
    {
        m_DebugRenderer->SetVoxelVisibleDistance(InVisibleDistance);
    }
}

void FProjectAcousticsModule::DebugRender(
    UWorld* world, UCanvas* canvas, const FVector& cameraPos, const FVector& cameraLook, float cameraFOV,
    bool shouldDrawStats, bool shouldDrawVoxels, bool shouldDrawProbes, bool shouldDrawDistances)
{
    if (!m_Triton)
    {
        return;
    }

    m_DebugRenderer->Render(
        world,
        canvas,
        cameraPos,
        cameraLook,
        cameraFOV,
        shouldDrawStats,
        shouldDrawVoxels,
        shouldDrawProbes,
        shouldDrawDistances);
}
#endif //! UE_BUILD_SHIPPING

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProjectAcousticsModule, ProjectAcoustics)
