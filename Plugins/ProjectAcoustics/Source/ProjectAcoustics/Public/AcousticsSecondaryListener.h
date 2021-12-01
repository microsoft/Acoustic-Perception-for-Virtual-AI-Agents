// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <array>
#include <complex>

#include "Engine/GameEngine.h"
#include "IAcoustics.h"
#include "Containers/Array.h"
#include "AcousticsSecondaryListener.generated.h"

DECLARE_STATS_GROUP(TEXT("Acoustics AI"), STATGROUP_AcousticsNPC, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(
TEXT("Acoustics NPC Query Count"), STAT_Acoustics_NpcQuery, STATGROUP_AcousticsNPC, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("NPC Reset Policy"), STAT_Acoustics_NpcPolicyReset, STATGROUP_AcousticsNPC, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("NPC Get Inputs"), STAT_Acoustics_NpcPolicyInput, STATGROUP_AcousticsNPC, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("NPC Policy Eval"), STAT_Acoustics_NpcPolicyEval, STATGROUP_AcousticsNPC, );

constexpr size_t kANGLE_COUNT = 360;
constexpr size_t kTABLE_LEN = 180;
constexpr size_t kNUM_DIRECTIONS = 6;

typedef struct SourceEnergy
{
    float direct_e; // total direct energy (including distance attenuation losses).
    float direct_azi;
    float direct_ele;
    int direct_idx; // idx into spatial array for direct energy
    float refl_0_e;
    float refl_90_e;
    float refl_180_e;
    float refl_270_e;
    float refl_down_e;
    float refl_up_e;
    FVector directDir;
} SourceEnergy;


UCLASS(
    config = Engine, hidecategories = Auto, AutoExpandCategories = Acoustics, BlueprintType, Blueprintable,
    ClassGroup = Acoustics)
    class PROJECTACOUSTICS_API UAcousticsSecondaryListener : public UActorComponent
{
    GENERATED_UCLASS_BODY()

public:
    // These are objects that the AI should engage and destroy
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    TArray<AActor*> TargetObjects;

    // These are objects that help define the noise floor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    TArray<AActor*> Ambiences;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = 0, ClampMin = 0, UIMax = 1, ClampMax = 1))
    float WalkSpeed = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool Enable3D = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool IgnoreAmbiences = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = -100, ClampMin = -100, UIMax = 100, ClampMax = 100))
    float SourceLoudnessOffsetDb = -80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = -60, ClampMin = -60, UIMax = 0, ClampMax = 0))
    float MinMaskingThresholdDb = -24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = -60, ClampMin = -60, UIMax = 0, ClampMax = 0))
    float MaxMaskingThresholdDb = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = -20, ClampMin = -20, UIMax = 20, ClampMax = 20))
    float ReverbConfusionThresholdDb = -10.0f;
    
    // How quiet of a sound can the agent hear?
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = -96, ClampMin = -96, UIMax = 20, ClampMax = 20))
    float ThresholdOfHearingDb = -50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics", meta = (UIMin = -96, ClampMin = -96, UIMax = 24, ClampMax = 24))
    float NoiseFloorDb = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool DrawDebugInfo = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    bool ConsiderAmbiences = true;

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    FVector GetAudioLookDirection() { return m_CurrentVelocity; }

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    AActor* GetLoudestActor();

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    float GetConfidence() { return m_Confidence; }

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    float GetDirectConfidence() { return m_DirectConfidence; }

    UFUNCTION(BlueprintCallable, Category = "Acoustics")
    float GetReflectionsConfidence() { return m_ReflectionsConfidence; }

    // AActor methods
    void BeginPlay() override;
    void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
    void PostEditChangeProperty(struct FPropertyChangedEvent& e) override;
#endif

private:
    void ResetPolicy();
    void AccumulatePolicyInputs();
    void AccumulateTargets();
    void AccumulateAmbiences();

    // Generate lookup tables.
    void GenerateMuLookupTable();
    void GenerateBetaMuLookupTable();

    // Fast evaluation methods.
    int DotProductToTableIndex(const FVector& a, const FVector& b);
    void ComputeReflectVector(const SourceEnergy& energy, FVector& reflectDirection, float& reflectMag);
    void ComputeAudibility(int targetIndex, FVector& direction, float& confidence, float& directPercent, float& reflectPercent);
    void AddEnergy(const SourceEnergy& energy);
    void SubEnergy(const SourceEnergy& energy);
    float Sigmoid(float x, float x_min, float x_max);
    SourceEnergy TritonParamsToSourceEnergy(const TritonAcousticParameters& params, float loudness_dbspl);

    void ComputeKernels();
    void AddEnergyToNoiseFloor(const SourceEnergy& energy);
    void EvaluatePolicy();

    void ApplyPolicy();
    void ShowDebugInfo();

    float CalculateLoudnessDb(const TritonAcousticParameters& tritonParams);

    // Global State
    IAcoustics* m_Acoustics;
    float m_TimeSinceLastUpdate = 100.0f;

    // Policy Decisions
    FVector m_CurrentVelocity = FVector::ZeroVector;
    float m_CurrentWalkSpeed = 0.0f;
    FVector m_TargetVelocity = FVector::ZeroVector;
    bool m_Confused = false;
    float m_Confidence = 1.0f;
    float m_DirectConfidence = 0.0f;
    float m_ReflectionsConfidence = 0.0f;

    // Policy inputs
    int m_LoudestTargetIndex = -1;
    TArray<TritonAcousticParameters> m_AllTargetParams;
    TArray<TritonAcousticParameters> m_AllAmbientParams;

    TArray<SourceEnergy> m_AllTargetEnergies;
    TArray<SourceEnergy> m_AllAmbientEnergies;

    // Reflections accumulation vector.
    std::array<float, kNUM_DIRECTIONS> m_reflectEnergy = { 0 };

    // Lookup tables for source spread and masking kernels.
    std::array<float, kTABLE_LEN> m_muTable = { 0 };
    std::array<float, kTABLE_LEN> m_betaMuTable = { 0 };

    // Fixed axes for each reflection direction.
    const std::array<FVector, kNUM_DIRECTIONS> m_reflectDirections = { { {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1} } };

    // Real-valued freq coefficients to compute lookup tables.
    const std::array<float, 4> m_betaMuCoeffs = { 0.72173f, -0.24202f, 0.097522f, -0.01711f };
    const std::array<float, 8> m_muCoeffs = { 0.72179f, -0.28516f, 0.19503f, -0.1007f, 0.038256f, -0.011879f, 0.005059f, -0.0027024f };

    std::array<float, kANGLE_COUNT> m_reverbNoiseEnergy = { 0 };
    std::array<float, kANGLE_COUNT> m_directNoiseEnergy = { 0 };

    std::array<float, kANGLE_COUNT> m_maskingSignal = { 0 };
    std::array<size_t, 4> m_reflIndices = { 0 };

    // Reverbs are in amplitude units (not dB)
    float m_TotalReverb = 0;
    // Per-channel reverb
    // 0/5 are up/down (Z axis)
    // 1/2 are X axis
    // 3/4 are Y axis
    float m_TotalVerb_chan0 = 0;
    float m_TotalVerb_chan1 = 0;
    float m_TotalVerb_chan2 = 0;
    float m_TotalVerb_chan3 = 0;
    float m_TotalVerb_chan4 = 0;
    float m_TotalVerb_chan5 = 0;
    float m_TotalBackgroundNoise = 0;
};