// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "TritonWwiseParams.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAcousticsRuntime, Log, All);
DECLARE_STATS_GROUP(TEXT("Project Acoustics"), STATGROUP_Acoustics, STATCAT_Advanced);

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules
 * within this plugin.
 */
class IAcoustics : public IModuleInterface
{
public:
    /**
     * Singleton-like access to this module's interface.  This is just for convenience!
     * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
     *
     * @return Returns singleton instance, loading the module on demand if needed
     */
    static inline IAcoustics& Get()
    {
        return FModuleManager::LoadModuleChecked<IAcoustics>("ProjectAcoustics");
    }

    /**
     * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
     *
     * @return True if the module is loaded and ready to use
     */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("ProjectAcoustics");
    }

    // Return value indicates success or failure
    // Must not be simultaneously accessed by two threads

    /**
     * Loads the ACE file that contains acoustic parameters for the scene
     * File must be located in the project's Content/Acoustics directory
     *
     * @return True if the ACE file is successfully loaded
     */
    virtual bool LoadAceFile(const FString& filePath, const float cacheScale) = 0;

    /**
     * Unload the currently loaded ACE file.
     */
    virtual void UnloadAceFile() = 0;

    /**
     * Register a new dynamic opening with acoustic system
     */
    virtual bool AddDynamicOpening(
        class UAcousticsDynamicOpening* opening, const FVector& center, const FVector& normal,
        const TArray<FVector>& vertices) = 0;

    /**
     * Unregister a dynamic opening with acoustic system
     */
    virtual bool RemoveDynamicOpening(class UAcousticsDynamicOpening* opening) = 0;

    /**
     * Update state of dynamic openings
     *
     * @return True on success.
     */
    virtual bool
    UpdateDynamicOpening(class UAcousticsDynamicOpening* opening, float dryAttenuationDb, float wetAttenuationDb) = 0;

    /**
     * Sets global design settings that are applied to all acoustic queries
     */
    virtual bool SetGlobalDesign(const UserDesign& params) = 0;

    /**
     * Given source & listener locations, compute the data used to set relevant settings in Wwise to reproduce
     * the acoustics at the listener location, taking design tweaks into account.
     * Data across all emitters is cached internally each tick.
     * See documentation for details.
     *
     * @param akObjectId The Wwise object ID that the sound source is attached to
     * @param sourceLocation The position of the sound source
     * @param listenerLocation The position of the listener/player/camera
     * @param designTweaks Optional struct to tweak the returned data based on designer preference. Pass in null to
     * use defaults. See the PerSourceDesignTweaks struct for documentation.
     *
     * @return True on success.
     */
    virtual bool UpdateWwiseParameters(
        const uint64_t akSourceObjectId, const FVector& sourceLocation, const FVector& listenerLocation,
        TritonWwiseParams& parameters, struct TritonDynamicOpeningInfo* outOpeningInfo) = 0;

    virtual bool QueryAcoustics(const int sourceId, const FVector& sourceLocation, const FVector& listenerLocation, TritonAcousticParameters& outParams) = 0;

    virtual bool UpdateOutdoorness(const FVector& listenerLocation) = 0;
    virtual float GetOutdoorness() const = 0;

    /**
     * Get cached parameters for all emitters that called UpdateWwiseParameters() this tick.
     */
    virtual const TMap<uint64_t, TritonWwiseParams>& GetCachedWwiseParameters() = 0;

    virtual bool PostTick() = 0;

    /**
     * Update Triton's internal listener distance data based on given listener location
     *
     * @return True on success.
     */
    virtual bool UpdateDistances(const FVector& listenerLocation) = 0;

    /**
     * Gives smoothed, precomputed distance in a given look direction from the listener's point of view.
     *
     * @return distance value
     */
    virtual bool QueryDistance(const FVector& lookDirection, float& outDistance) = 0;

    /**
     * Convert Triton's propagation delay to propagation distance in centimeters
     */
    virtual float TritonDelayToUnrealDistance(float delay) const = 0;

    /**
     * Convert Triton's spherical angles to Unreal direction vector
     *
     * @return Unreal vector corresponding to the given Triton direction.
     */
    virtual FVector TritonSphericalToUnrealCartesian(float tritonAzimuth, float tritonElevation) const = 0;

    /**
     * Used for ACE streaming. For the given player position, update which parts of the ACE file are loaded in memory
     */
    virtual void UpdateLoadedRegion(
        const FVector& playerPosition, const FVector& tileSize, const bool forceUpdate,
        const bool unloadProbesOutsideTile, const bool blockOnCompletion) = 0;

#if !UE_BUILD_SHIPPING
    virtual void SetEnabled(bool isEnabled) = 0;
    virtual void
    UpdateSourceDebugInfo(uint64_t sourceID, bool shouldDraw, FName displayName, bool isLoudest, bool isConfused = false) = 0;
    virtual void DebugRender(
        class UWorld* world, class UCanvas* canvas, const FVector& cameraPos, const FVector& cameraLook,
        float cameraFOV, bool shouldDrawStats, bool shouldDrawVoxels, bool shouldDrawProbes,
        bool shouldDrawDistances) = 0;
    virtual void UpdateConfidenceValues(FVector direction, float confidence) = 0;
    virtual void SetVoxelVisibleDistance(const float InVisibleDistance) = 0;
#endif
};
