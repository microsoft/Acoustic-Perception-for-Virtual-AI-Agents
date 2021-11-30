// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "Modules/ModuleManager.h"
#include "IAcoustics.h"
#include "UnrealTritonHooks.h"
#include "TritonWwiseParams.h"
#include "TritonDebugInterface.h"

#if !UE_BUILD_SHIPPING
class FProjectAcousticsDebugRender;
#endif

class FProjectAcousticsModule : public IAcoustics
{
public:
    FProjectAcousticsModule();

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // IAcoustics
    virtual bool LoadAceFile(const FString& filePath, const float cacheScale) override;
    virtual void UnloadAceFile() override;

    virtual bool AddDynamicOpening(
        class UAcousticsDynamicOpening* opening, const FVector& center, const FVector& normal,
        const TArray<FVector>& vertices) override;
    virtual bool RemoveDynamicOpening(class UAcousticsDynamicOpening* opening) override;
    virtual bool UpdateDynamicOpening(
        class UAcousticsDynamicOpening* opening, float dryAttenuationDb, float wetAttenuationDb) override;

    virtual bool SetGlobalDesign(const UserDesign& params) override;

    virtual bool UpdateWwiseParameters(
        const uint64_t akSourceObjectId, const FVector& sourceLocation, const FVector& listenerLocation,
        TritonWwiseParams& parameters, struct TritonDynamicOpeningInfo* outOpeningInfo) override;
    virtual bool QueryAcoustics(const int sourceId, const FVector& sourceLocation, const FVector& listenerLocation, TritonAcousticParameters& outParams) override;
    virtual const TMap<uint64_t, TritonWwiseParams>& GetCachedWwiseParameters() override;
    virtual bool UpdateOutdoorness(const FVector& listenerLocation) override;
    virtual float GetOutdoorness() const override;

    virtual bool PostTick() override;

    virtual bool UpdateDistances(const FVector& listenerLocation) override;
    virtual bool QueryDistance(const FVector& lookDirection, float& outDistance) override;
    virtual float TritonDelayToUnrealDistance(float delay) const override;
    virtual FVector TritonSphericalToUnrealCartesian(float azimuth, float elevation) const override;
    virtual void UpdateLoadedRegion(
        const FVector& playerPosition, const FVector& tileSize, const bool forceUpdate,
        const bool unloadProbesOutsideTile, const bool blockOnCompletion) override;

#if !UE_BUILD_SHIPPING
    virtual void SetEnabled(bool isEnabled) override;

    virtual void
    UpdateSourceDebugInfo(uint64_t sourceID, bool shouldDraw, FName displayName, bool isLoudest, bool isConfused) override;
    virtual void UpdateConfidenceValues(FVector direction, float confidence) override;

    // Allow to set the exposed voxel distance
    void SetVoxelVisibleDistance(const float InVisibleDistance) override;

    virtual void DebugRender(
        UWorld* world, UCanvas* canvas, const FVector& cameraPos, const FVector& cameraLook, float cameraFOV,
        bool shouldDrawStats, bool shouldDrawVoxels, bool shouldDrawProbes, bool shouldDrawDistances) override;

    TritonRuntime::TritonAcousticsDebug* GetTritonDebugInstance() const;
    bool IsAceFileLoaded() const
    {
        return m_AceFileLoaded;
    }

    int64 GetMemoryUsed() const
    {
        return m_TritonMemHook != nullptr ? m_TritonMemHook->GetTotalMemoryUsed() : 0;
    }

    int64 GetDiskBytesRead() const
    {
        return m_TritonIOHook != nullptr ? m_TritonIOHook->GetBytesRead() : 0;
    }

#endif

private:
    // Triton members
    TritonRuntime::TritonAcoustics* m_Triton;
    bool m_TritonInstanceCreated;
    bool m_AceFileLoaded;
    TMap<uint64_t, TritonWwiseParams> m_WwiseParamsCache;
    FVector m_LastLoadCenterPosition;
    FVector m_LastLoadTileSize;
    TUniquePtr<TritonRuntime::FTritonMemHook> m_TritonMemHook;
    TUniquePtr<TritonRuntime::FTritonLogHook> m_TritonLogHook;
    TUniquePtr<TritonRuntime::FTritonUnrealIOHook> m_TritonIOHook;
    TUniquePtr<TritonRuntime::FTritonAsyncTaskHook> m_TritonTaskHook;
    bool m_IsOutdoornessStale;
    float m_CachedOutdoorness;
    UserDesign m_GlobalDesign;

#if !UE_BUILD_SHIPPING
    bool m_IsEnabled;
    TUniquePtr<FProjectAcousticsDebugRender> m_DebugRenderer;
#endif

    // Helpers
    bool GetAcousticParameters(
        const FVector& sourceLocation, const FVector& listenerLocation, TritonAcousticParameters& params,
        TritonDynamicOpeningInfo* outOpeningInfo, TritonRuntime::QueryDebugInfo* outDebugInfo = nullptr);
    void CollectPluginData(const TritonWwiseParams& params);
};

// Statistics hooks
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Wwise Params"), STAT_Acoustics_UpdateWwiseParams, STATGROUP_Acoustics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Query Acoustics"), STAT_Acoustics_Query, STATGROUP_Acoustics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Query Outdoorness"), STAT_Acoustics_QueryOutdoorness, STATGROUP_Acoustics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Load Region"), STAT_Acoustics_LoadRegion, STATGROUP_Acoustics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Load Ace File"), STAT_Acoustics_LoadAce, STATGROUP_Acoustics, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear Ace File"), STAT_Acoustics_ClearAce, STATGROUP_Acoustics, );
