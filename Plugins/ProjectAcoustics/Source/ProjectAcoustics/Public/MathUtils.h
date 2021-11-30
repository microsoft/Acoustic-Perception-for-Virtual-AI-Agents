// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once
#include "CoreMinimal.h"
#include "TritonVector.h"

namespace TritonRuntime
{
    // Vector type conversion
    template <typename T>
    static Triton::Vec3f ToTritonVector(const T& t)
    {
        return Triton::Vec3f{{static_cast<float>(t.X), static_cast<float>(t.Y), static_cast<float>(t.Z)}};
    }

    template <typename T>
    static FVector ToFVector(const T& t)
    {
        return FVector{static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z)};
    }

    // Scale conversion
    static float DbToAmplitude(float decibels)
    {
        return pow(10.0f, decibels / 20.0f);
    }

    static float AmplitudeToDb(float amplitude)
    {
        // protect against 0 amplitude which throws exception - clamp at -200dB
        return 10 * log10(amplitude * amplitude + 1e-20f);
    }

    // Conversion routines:
    //    Unreal's engine is left-handed Y-up, centimeters
    //    However, Unreal's FBX import & export is left-handed Z-up, centimeters
    //    Triton is right-handed Z-up, meters
    //    Therefore, conversion between Triton and Unreal's imported FBX coordinates is simply:
    //        Negate Y-axis and scale
    constexpr auto c_UnrealToTritonScale = 0.01f;
    constexpr auto c_TritonToUnrealScale = 1.0f / c_UnrealToTritonScale;

    // Position transformations.
    // If your game has some additional transform during gameplay with respect to the transform during bake,
    // modify these functions based on the additional transform matrix.
    static FVector TritonPositionToUnreal(const FVector& vec)
    {
        return FVector{vec.X * c_TritonToUnrealScale, -vec.Y * c_TritonToUnrealScale, vec.Z * c_TritonToUnrealScale};
    }

    static FVector UnrealPositionToTriton(const FVector& vec)
    {
        return FVector{vec.X * c_UnrealToTritonScale, -vec.Y * c_UnrealToTritonScale, vec.Z * c_UnrealToTritonScale};
    }

    // Direction transformations. Need to preserve length of vector and assume a unit vector as input.
    // If your game has an additional transform during gameplay with respect to the transform during bake,
    // modify these functions based on ONLY the rotational component of the transform.
    static FVector UnrealDirectionToTriton(const FVector& vec)
    {
        return FVector{vec.X, -vec.Y, vec.Z};
    }

    static FVector TritonDirectionToUnreal(const FVector& vec)
    {
        return FVector{vec.X, -vec.Y, vec.Z};
    }

} // namespace TritonRuntime