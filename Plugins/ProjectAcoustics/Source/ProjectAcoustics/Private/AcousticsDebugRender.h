// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "CoreMinimal.h"
#include "TritonWwiseParams.h"
#include "QueryDebugInfo.h"

enum class AAFaceDirection
{
    X,
    Y,
    Z
};

class UWorld;
class UCanvas;

class FProjectAcousticsDebugRender
{
private:
    //! Additional debug information about emitter not contained in parameters cache
    struct EmitterDebugInfo
    {
        FName DisplayName;
        uint64_t SourceID;
        FVector SourceLocation;
        FVector ListenerLocation;
        bool DidQuerySucceed;
        TritonWwiseParams wwiseParams;
        TritonRuntime::QueryDebugInfo queryDebugInfo;
        bool ShouldDraw;
        bool IsLoudest;
        bool IsConfused;

        // Used to help us cull inactive sources
        bool hadUpdate;
    };

    class FProjectAcousticsModule* m_Acoustics;
    UWorld* m_World;
    UCanvas* m_Canvas;
    FVector m_CameraPos;
    FVector m_CameraLook;
    float m_CameraFOV;
    FString m_LoadedFilename;
    TMap<uint64_t, EmitterDebugInfo> m_DebugCache;
// Ifdef out for non-unity build
#if !UE_BUILD_SHIPPING
    void DrawDirection(const EmitterDebugInfo& info, const TritonWwiseParams& params, const FColor& arrowColor);
    void DrawStats();
    void DrawVoxels();
    void DrawProbes();
    void DrawDistances();
    void DrawSources();
#endif
    // Exposed voxel distance
    float m_VoxelVisibleDistance = 1000.f;

    FVector m_ConfidentDirection = FVector::ZeroVector;
    float m_Confidence = 0;

public:
    FProjectAcousticsDebugRender(FProjectAcousticsModule* owner);

// Ifdef out for non-unity build
#if !UE_BUILD_SHIPPING
    void SetLoadedFilename(FString fileName);
    bool UpdateSourceAcoustics(
        uint64_t sourceID, FVector sourceLocation, FVector listenerLocation, bool didQuerySucceed,
        const TritonWwiseParams& wwiseParams, const TritonRuntime::QueryDebugInfo& queryDebugInfo);
    bool UpdateSourceDebugInfo(uint64_t sourceID, bool shouldDraw, FName displayName, bool isLoudest, bool isConfused);
    void UpdateConfidenceVector(FVector direction, float confidence);
    bool Render(
        UWorld* world, UCanvas* canvas, const FVector& cameraPos, const FVector& cameraLook, float cameraFOV,
        bool shouldDrawStats, bool shouldDrawVoxels, bool shouldDrawProbes, bool shouldDrawDistances);
    // Normal needs to point in an axis-aligned direction. Undefined behavior otherwise.
    static void DrawDebugAARectangle(
        const UWorld* inWorld, const FVector& faceCenter, const FVector& faceSize, AAFaceDirection dir,
        const FColor& color);
#endif

    void SetVoxelVisibleDistance(const float InVisibleDistance)
    {
        m_VoxelVisibleDistance = InVisibleDistance;
    }
};
