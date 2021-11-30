// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <array>
#include <complex>
#define _MANAGED 0
#define _M_CEE 0
#define _XM_SSE_INTRINSICS_ 1
#include "XDSP.h"
#undef _M_CEE
#undef _MANAGED


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

// MICHEM TODO
// For perf testing, set to 360, take measurement
// Then set to 48, take measurement
// The perf at 48 *should be* roughly what we would get at 360 if we optimized the FFTs.
// If time, pull in FFTW
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

void RVRVAdd(float* signal1, float* signal2, int n);

// Complex Vector, Complex Vector addition
void CVCVAdd(std::complex<float>* signal1, std::complex<float>* signal2, int n);

// This routine performs frequency-domain convolution: Element-wise complex product using SSE3.
void CVCVMult(std::complex<float>* signal, std::complex<float>* filter, int n);

template <size_t N>
class FftWrapper
{
public:
    FftWrapper()
    {
        assert(XDSP::ISPOWEROF2(N));

        if (FftWrapper<N>::numBuffers == 0)
        {
            FftWrapper<N>::unityTable = static_cast<DirectX::XMVECTOR*>(_aligned_malloc(N * sizeof(DirectX::XMVECTOR), 16));
            FftWrapper<N>::unityTableHalf = static_cast<DirectX::XMVECTOR*>(_aligned_malloc(N / 2 * sizeof(DirectX::XMVECTOR), 16));
            XDSP::FFTInitializeUnityTable(FftWrapper<N>::unityTable, N);
            XDSP::FFTInitializeUnityTable(FftWrapper<N>::unityTableHalf, N / 2);
        }
        FftWrapper<N>::numBuffers++;

        this->m_real_temp = static_cast<float*>(_aligned_malloc((N + 4) * sizeof(float), 16));
        this->m_imag_temp = static_cast<float*>(_aligned_malloc((N + 4) * sizeof(float), 16));
        this->m_temp1 = static_cast<float*>(_aligned_malloc((N + 4) * sizeof(float), 16));
        this->m_temp2 = static_cast<float*>(_aligned_malloc((N + 4) * sizeof(float), 16));

        int nSpectrum = N / 2 + 1;
        this->m_cplus = static_cast<std::complex<float>*>(_aligned_malloc(nSpectrum * sizeof(std::complex<float>), 16));
        this->m_cminus = static_cast<std::complex<float>*>(_aligned_malloc(nSpectrum * sizeof(std::complex<float>), 16));
        this->m_ztemp = static_cast<std::complex<float>*>(_aligned_malloc(N / 2 * sizeof(std::complex<float>), 16));

        for (int i = 0; i < nSpectrum; ++i)
        {
            m_cplus[i].real(.5f * (1.f - std::sinf((2 * PI * i) * FftWrapper<N>::oneByN)));
            m_cplus[i].imag(.5f * (-std::cosf((2 * PI * i) * FftWrapper<N>::oneByN)));

            m_cminus[i].real(.5f * (1.f + std::sinf((2 * PI * i) * FftWrapper<N>::oneByN)));
            m_cminus[i].imag(.5f * (std::cosf((2 * PI * i) * FftWrapper<N>::oneByN)));
        }
    }

    ~FftWrapper()
    {
        _aligned_free(this->m_real_temp);
        _aligned_free(this->m_imag_temp);
        _aligned_free(this->m_temp1);
        _aligned_free(this->m_temp2);
        _aligned_free(this->m_cplus);
        _aligned_free(this->m_cminus);
        _aligned_free(this->m_ztemp);

        FftWrapper<N>::numBuffers--;
        if (FftWrapper<N>::numBuffers == 0)
        {
            _aligned_free(FftWrapper<N>::unityTable);
            _aligned_free(FftWrapper<N>::unityTableHalf);
        }
    }

    constexpr size_t Size() const
    {
        return N;
    }

    void Fft(float* input, std::complex<float>* output)
    {
        int nSpectrum = N / 2 + 1;
        memset(m_real_temp, 0, nSpectrum * sizeof(float));
        memset(m_imag_temp, 0, nSpectrum * sizeof(float));

        // Interleave data as real and imaginary parts, do FFT
        for (int i = 0; i < N / 2; i++)
        {
            m_real_temp[i] = input[2 * i];
            m_imag_temp[i] = input[2 * i + 1];
        }

        XDSP::FFT((XDSP::XMVECTOR*)m_real_temp, (XDSP::XMVECTOR*)m_imag_temp, FftWrapper<N>::unityTableHalf, N / 2);
        XDSP::FFTUnswizzle((XDSP::XMVECTOR*)m_temp1, (XDSP::XMVECTOR*)m_real_temp, FftWrapper<N>::log2N - 1);
        XDSP::FFTUnswizzle((XDSP::XMVECTOR*)m_temp2, (XDSP::XMVECTOR*)m_imag_temp, FftWrapper<N>::log2N - 1);

        float t1 = m_temp1[0];
        float t2 = m_temp2[0];

        for (int i = 1; i < N / 2; i++)
        {
            m_ztemp[i].real(m_temp1[i]);
            m_ztemp[i].imag(m_temp2[i]);

            output[i].real(m_temp1[N / 2 - i]);
            output[i].imag(-m_temp2[N / 2 - i]);
        }

        CVCVMult(m_ztemp, m_cplus, N / 2);
        CVCVMult(output, m_cminus, N / 2);
        CVCVAdd(output, m_ztemp, N / 2);

        // DC bin
        output[0].real(t1 + t2);
        output[0].imag(0);

        // Nyquist bin
        output[N / 2].real(t1 - t2);
        output[N / 2].imag(0);
    }

    void Ifft(std::complex<float>* input, float* output)
    {
        m_real_temp[0] = input[0].real();
        m_imag_temp[0] = 0;

        // Fill conjugated, then do FFT. The result need not be conjugated, since it is real.
        int nSpectrum = N / 2 + 1;
        for (int i = 1; i < nSpectrum; i++)
        {
            m_real_temp[i] = input[i].real();
            m_imag_temp[i] = -input[i].imag();

            m_real_temp[N - i] = m_real_temp[i];
            m_imag_temp[N - i] = -m_imag_temp[i]; // Conjugate, mirrored
        }

        m_imag_temp[N / 2] = 0;

        XDSP::FFT((XDSP::XMVECTOR*)m_real_temp, (XDSP::XMVECTOR*)m_imag_temp, FftWrapper<N>::unityTable, N);
        XDSP::FFTUnswizzle((XDSP::XMVECTOR*)output, (XDSP::XMVECTOR*)m_real_temp, FftWrapper<N>::log2N);

        const XDSP::XMVECTOR scale = _mm_set_ps1(1.0f / N);
        XDSP::XMVECTOR* O = (XDSP::XMVECTOR*)output;

        for (int i = 0; i < N / 4; i++)
        {
            O[i] = _mm_mul_ps(O[i], scale);
        }
    }

    static DirectX::XMVECTOR* unityTable;
    static DirectX::XMVECTOR* unityTableHalf;
    static size_t numBuffers;
    static size_t log2N;
    static float oneByN;

private:
    float* m_real_temp;
    float* m_imag_temp;
    float* m_temp1;
    float* m_temp2;

    std::complex<float>* m_cplus;
    std::complex<float>* m_cminus;
    std::complex<float>* m_ztemp;
};

DirectX::XMVECTOR* FftWrapper<kANGLE_COUNT>::unityTable = nullptr;
DirectX::XMVECTOR* FftWrapper<kANGLE_COUNT>::unityTableHalf = nullptr;
size_t FftWrapper<kANGLE_COUNT>::numBuffers = 0;
size_t FftWrapper<kANGLE_COUNT>::log2N = static_cast<size_t>(std::log2(static_cast<float>(kANGLE_COUNT)));
float FftWrapper<kANGLE_COUNT>::oneByN = 1.0f / static_cast<float>(kANGLE_COUNT);


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
    void ComputeAudibilityNEW(int targetIndex, FVector& direction, float& confidence, float& directPercent, float& reflectPercent);
    void AddEnergyNEW(const SourceEnergy& energy);
    void SubEnergyNEW(const SourceEnergy& energy);
    float SigmoidNEW(float x, float x_min, float x_max);
    SourceEnergy TritonParamsToSourceEnergy(const TritonAcousticParameters& params, float loudness_dbspl);

    void ComputeKernels();
    void AddEnergyToNoiseFloor(const SourceEnergy& energy);
    void EvaluateNewPolicy();

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