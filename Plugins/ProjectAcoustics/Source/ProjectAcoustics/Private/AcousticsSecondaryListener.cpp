// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "AcousticsSecondaryListener.h"
#include "AcousticsSecondarySource.h"
#include "AcousticsAudioComponent.h"
#include "GameFramework/Character.h"

#include <limits>


constexpr float kPI = static_cast<float>(3.14159265358979323846);

DEFINE_STAT(STAT_Acoustics_NpcPolicyInput);
DEFINE_STAT(STAT_Acoustics_NpcPolicyReset);
DEFINE_STAT(STAT_Acoustics_NpcPolicyEval);
DEFINE_STAT(STAT_Acoustics_NpcQuery);

namespace {

    float dbToEnergy(float db)
    {
        return FMath::Pow(10, db / 10.0f);
    }
    float dbToAmp(float db)
    {
        return FMath::Pow(10.0f, db / 20.0f);
    }
    float ampToDb(float amp)
    {
        return 20 * FMath::LogX(10.0f, amp);
    }

    float energyToDb(float e)
    {
        return 10.0f * std::log10f(e);
    }

    // Generate a kernel (in energy units) that is convolved agaisnt comparitive signals to produce a masking function.
    void generateMaskingKernel(float* kernel, size_t len, float dBStart, float dB90Deg)
    {
        // Create a cos^2(theta) window function.
        float dBRange = dBStart - dB90Deg;
        float TwoPiByLen = 2.0f * kPI / static_cast<float>(len);
        float e90Deg = FMath::Pow(10.0f, dB90Deg / 10.0f);
        for (size_t i = 0; i < len; ++i)
        {
            float cosTheta = FMath::Cos(static_cast<float>(i) * TwoPiByLen);
            if (cosTheta > 0.0f)
            {
                float dBAtIdxI = cosTheta * cosTheta * dBRange + dB90Deg;
                
                // Convert to energy units.
                kernel[i] = FMath::Pow(10.0f, dBAtIdxI / 10.0f);
            }
            else
            {
                kernel[i] = e90Deg;
            }
        }
    }

    // Generate a kernel (in energy units) that is convolved agaisnt comparitive signals to produce an energy-preserving source spread function.
    void generateSourceSpreadKernel(float* kernel, size_t len)
    {
        // Create a cos^2(theta) window function.
        float TwoPiByLen = 2.0f * kPI / static_cast<float>(len);
        float sum = 0.0f;
        for (size_t i = 0; i < len; ++i)
        {
            float cosTheta = FMath::Cos(static_cast<float>(i) * TwoPiByLen);
            if (cosTheta > 0.0f)
            {
                kernel[i] = cosTheta * cosTheta;
                sum += kernel[i];
            }
            else
            {
                kernel[i] = 0.0f;
            }
        }

        // Preserve energy by normalizing the kernel.
        float recip_sum = 1.0f / (sum + 1e-10f);
        for (size_t i = 0; i < len; ++i)
        {
            kernel[i] *= recip_sum;
        }
    }

    // Print signal to debug log.
    void printSignal(float* signal, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            UE_LOG(LogAcousticsRuntime, Display, TEXT("[%d] %f"), i, signal[i]);
        }
    }

    // Print complex signal to debug log.
    void printComplexSignal(std::complex<float>* signal, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            UE_LOG(LogAcousticsRuntime, Display, TEXT("[%d] %f + %fi"), i, signal[i].real(), signal[i].imag());
        }
    }

}  // namespace

UAcousticsSecondaryListener::UAcousticsSecondaryListener(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
    m_Acoustics = nullptr;
}

void UAcousticsSecondaryListener::BeginPlay()
{
    Super::BeginPlay();
    if (IAcoustics::IsAvailable())
    {
        // cache module instance
        m_Acoustics = &(IAcoustics::Get());
    }

    // Initialize directional reflection indices.
    for (size_t i = 0; i < 4; ++i)
    {
        m_reflIndices[i] = static_cast<size_t>(static_cast<float>(i * kANGLE_COUNT) / 4.0f + 0.5f);
    }

    // Initialize kernels.
    ComputeKernels();
    GenerateMuLookupTable();
    GenerateBetaMuLookupTable();
}

SourceEnergy UAcousticsSecondaryListener::TritonParamsToSourceEnergy(const TritonAcousticParameters& params, float loudness_dbspl)
{
    SourceEnergy s;
    const float c = 343.0f;
    float atten_db = 20.0f * FMath::LogX(10, 1.0f / (params.DirectDelay * c));
    s.direct_e = FMath::Pow(10.0f, (params.DirectLoudnessDB + atten_db + loudness_dbspl) / 10.0f);
    s.direct_azi = params.DirectAzimuth;
    s.direct_idx = int(params.DirectAzimuth / 360.0f * static_cast<float>(kANGLE_COUNT - 1) + 0.5f);
    s.refl_0_e = FMath::Pow(10.0f, (params.ReflLoudnessDB_Channel_1 + loudness_dbspl) / 10.0f);
    s.refl_90_e = FMath::Pow(10.0f, (params.ReflLoudnessDB_Channel_2 + loudness_dbspl) / 10.0f);
    s.refl_180_e = FMath::Pow(10.0f, (params.ReflLoudnessDB_Channel_3 + loudness_dbspl) / 10.0f);
    s.refl_270_e = FMath::Pow(10.0f, (params.ReflLoudnessDB_Channel_4 + loudness_dbspl) / 10.0f);
    
    if (Enable3D)
    {
        s.direct_ele = params.DirectElevation;
        s.refl_down_e = FMath::Pow(10.0f, (params.ReflLoudnessDB_Channel_0 + loudness_dbspl) / 10.0f);
        s.refl_up_e = FMath::Pow(10.0f, (params.ReflLoudnessDB_Channel_5 + loudness_dbspl) / 10.0f);
    }
    else
    {
        s.direct_ele = 90;
        s.refl_down_e = 0;
        s.refl_up_e = 0;
    }
    
    float aziRad = s.direct_azi * kPI / 180.0f;
    float eleRad = s.direct_ele * kPI / 180.0f;
    float sinEleRad = std::sinf(eleRad);
    s.directDir = { std::cosf(aziRad) * sinEleRad, std::sinf(aziRad) * sinEleRad, std::cosf(eleRad) };
    return s;
}

// Note: This function will be called after all source component ticks.
void UAcousticsSecondaryListener::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // No acoustics module loaded. Skip
    if (!m_Acoustics)
    {
        return;
    }

    // Minimize number of updates to mimic being updated in a background thread
    m_TimeSinceLastUpdate += DeltaTime;
    if (m_TimeSinceLastUpdate >= 0.15)
    {
        // Enough time has elapsed. Run the policy update loop

#if !UE_BUILD_SHIPPING
        SET_DWORD_STAT(STAT_Acoustics_NpcQuery, 0);
#endif

        ResetPolicy();
        AccumulatePolicyInputs();
        {
#if !UE_BUILD_SHIPPING
            SCOPE_CYCLE_COUNTER(STAT_Acoustics_NpcPolicyEval);
#endif
            EvaluatePolicy();
        
        }
    }
    // Always update debug info, otherwise viz will flicker
    if (DrawDebugInfo)
    {
        ShowDebugInfo();
    }
    // Always apply latest policy to keep the character in motion
    ApplyPolicy();
}

static constexpr float SpeedOfSound = 340.0f;
float UAcousticsSecondaryListener::CalculateLoudnessDb(const TritonAcousticParameters& tritonParams)
{
    if (tritonParams.DirectDelay == TritonAcousticParameters::FailureCode || tritonParams.DirectLoudnessDB == TritonAcousticParameters::FailureCode)
    {
        return std::numeric_limits<float>::lowest();
    }

    // Calculate 1/r distance-based attenuation
    const float directDistance = SpeedOfSound * tritonParams.DirectDelay;
    const float distanceAttenuation = 20 * FMath::LogX(10, 1.0f / directDistance);
    return tritonParams.DirectLoudnessDB + distanceAttenuation;
}

void UAcousticsSecondaryListener::GenerateMuLookupTable()
{
    const float piByN = kPI / static_cast<float>(m_muTable.size());

    for (size_t i = 0; i < m_muTable.size(); ++i)
    {
        float v = m_muCoeffs[0] * 0.5f;
        for (size_t k = 1; k < m_muCoeffs.size(); ++k)
        {
            const float theta = static_cast<float>(k * (i + m_muTable.size())) * piByN;
            v = v + m_muCoeffs[k] * std::cosf(theta);
        }
        m_muTable[i] = v;
    }
}

void UAcousticsSecondaryListener::GenerateBetaMuLookupTable()
{
    const float piByN = kPI / static_cast<float>(m_betaMuTable.size());

    for (size_t i = 0; i < m_betaMuTable.size(); ++i)
    {
        float v = m_betaMuCoeffs[0] * 0.5f;
        for (size_t k = 1; k < m_betaMuCoeffs.size(); ++k)
        {
            const float theta = static_cast<float>(k * (i + m_betaMuTable.size())) * piByN;
            v = v + m_betaMuCoeffs[k] * std::cosf(theta);
        }
        m_betaMuTable[i] = v;
    }
}

int UAcousticsSecondaryListener::DotProductToTableIndex(const FVector& a, const FVector& b)
{
    // Assumes that |a| = |b| = 1.
    constexpr float tableLenByPi = static_cast<float>(kTABLE_LEN - 1) / kPI;
    return static_cast<int>(std::acosf(a.X * b.X + a.Y * b.Y + a.Z * b.Z) * tableLenByPi);
}

void UAcousticsSecondaryListener::ComputeReflectVector(const SourceEnergy& energy, FVector& reflectDirection, float& reflectMag)
{
    FVector reflectVector = { energy.refl_0_e - energy.refl_180_e, energy.refl_90_e - energy.refl_270_e, energy.refl_up_e - energy.refl_down_e };
    reflectMag = std::sqrtf(reflectVector.X * reflectVector.X + reflectVector.Y * reflectVector.Y + reflectVector.Z * reflectVector.Z);
    float recipMag = 1.0f / (reflectMag + FLT_MIN);
    reflectDirection = { reflectVector.X * recipMag, reflectVector.Y * recipMag, reflectVector.Z * recipMag };
}

void UAcousticsSecondaryListener::ResetPolicy()
{
#if !UE_BUILD_SHIPPING
    SCOPE_CYCLE_COUNTER(STAT_Acoustics_NpcPolicyReset);
#endif
    m_LoudestTargetIndex = -1;
    
    m_TotalBackgroundNoise = 0;
    m_TotalReverb = 0;
    m_TotalVerb_chan0 = 0;
    m_TotalVerb_chan1 = 0;
    m_TotalVerb_chan2 = 0;
    m_TotalVerb_chan3 = 0;
    m_TotalVerb_chan4 = 0;
    m_TotalVerb_chan5 = 0;

    m_AllTargetParams.Empty();
    m_AllAmbientParams.Empty();
    
    m_AllTargetEnergies.Empty();
    m_AllAmbientEnergies.Empty();
    
    m_reverbNoiseEnergy.fill(1);
    m_directNoiseEnergy.fill(0);
    m_maskingSignal.fill(0);

    m_DirectConfidence = 0.0f;
    m_ReflectionsConfidence = 0.0f;

    m_reflectEnergy.fill(0);
}

TritonAcousticParameters CreateFailedParams()
{
    TritonAcousticParameters params;
    params.DirectAzimuth = TritonAcousticParameters::FailureCode;
    params.DirectDelay = TritonAcousticParameters::FailureCode;
    params.DirectElevation = TritonAcousticParameters::FailureCode;
    params.DirectLoudnessDB = TritonAcousticParameters::FailureCode;
    params.EarlyDecayTime = TritonAcousticParameters::FailureCode;
    params.ReflectionsDelay = TritonAcousticParameters::FailureCode;
    params.ReflectionsLoudnessDB = TritonAcousticParameters::FailureCode;
    params.ReflLoudnessDB_Channel_0 = TritonAcousticParameters::FailureCode;
    params.ReflLoudnessDB_Channel_1 = TritonAcousticParameters::FailureCode;
    params.ReflLoudnessDB_Channel_2 = TritonAcousticParameters::FailureCode;
    params.ReflLoudnessDB_Channel_3 = TritonAcousticParameters::FailureCode;
    params.ReflLoudnessDB_Channel_4 = TritonAcousticParameters::FailureCode;
    params.ReflLoudnessDB_Channel_5 = TritonAcousticParameters::FailureCode;
    params.ReverbTime = TritonAcousticParameters::FailureCode;
    return params;
}

void UAcousticsSecondaryListener::AddEnergyToNoiseFloor(const SourceEnergy& energy)
{
    m_reverbNoiseEnergy[m_reflIndices[0]] += energy.refl_0_e;
    m_reverbNoiseEnergy[m_reflIndices[1]] += energy.refl_90_e;
    m_reverbNoiseEnergy[m_reflIndices[2]] += energy.refl_180_e;
    m_reverbNoiseEnergy[m_reflIndices[3]] += energy.refl_270_e;

    m_directNoiseEnergy[energy.direct_idx] += energy.direct_e;
}

void UAcousticsSecondaryListener::AddEnergy(const SourceEnergy& energy)
{
    m_reflectEnergy[0] += energy.refl_up_e;
    m_reflectEnergy[1] += energy.refl_0_e;
    m_reflectEnergy[2] += energy.refl_90_e;
    m_reflectEnergy[3] += energy.refl_180_e;
    m_reflectEnergy[4] += energy.refl_270_e;
    m_reflectEnergy[5] += energy.refl_down_e;
}

void UAcousticsSecondaryListener::SubEnergy(const SourceEnergy& energy)
{
    m_reflectEnergy[0] -= energy.refl_up_e;
    m_reflectEnergy[1] -= energy.refl_0_e;
    m_reflectEnergy[2] -= energy.refl_90_e;
    m_reflectEnergy[3] -= energy.refl_180_e;
    m_reflectEnergy[4] -= energy.refl_270_e;
    m_reflectEnergy[5] -= energy.refl_down_e;
}

float UAcousticsSecondaryListener::Sigmoid(float x, float x_min, float x_max)
{
    // Scaling the sigmoid by this value ensures S(-1) = 0.05 and S(1) = 0.95.
    const float K = 2.945f;

    float halfRange = (x_max - x_min) * 0.5f;
    float midpoint = (x_max + x_min) * 0.5f;
    float v = (x - midpoint) / halfRange * K;
    return 1.0f / (1.0f + std::expf(-v));
}

void UAcousticsSecondaryListener::ComputeAudibility(int targetIndex, FVector& direction, float& confidence, float& directPercent, float& reflectPercent)
{
    SubEnergy(m_AllTargetEnergies[targetIndex]);

    // Compute reflection vector.
    FVector reflectDirection;
    float reflectMag;
    ComputeReflectVector(m_AllTargetEnergies[targetIndex], reflectDirection, reflectMag);

    // Set to 1 for threshold-of-hearing SMR when no additional sources present.
    float directMaskEnergy = 1;
    float reflectMaskEnergy = 1;

    // Compute direct energy contributions (E(L^k_d) * mu(s_k * s_d) and E(L^k_d) * mu(s_k * s_r) terms).
    for (int i = 0; i < m_AllTargetEnergies.Num(); ++i)
    {
        // Skip `this` target.
        if (i == targetIndex)
        {
            continue;
        }
        
        int directMuIndex = DotProductToTableIndex(m_AllTargetEnergies[targetIndex].directDir, m_AllTargetEnergies[i].directDir);
        int reflectMuIndex = DotProductToTableIndex(reflectDirection, m_AllTargetEnergies[i].directDir);
        
        directMaskEnergy += m_AllTargetEnergies[i].direct_e * m_muTable[directMuIndex];
        reflectMaskEnergy += m_AllTargetEnergies[i].direct_e * m_muTable[reflectMuIndex];
    }
    for (int i = 0; i < m_AllAmbientEnergies.Num(); ++i)
    {
        int directMuIndex = DotProductToTableIndex(m_AllTargetEnergies[targetIndex].directDir, m_AllAmbientEnergies[i].directDir);
        int reflectMuIndex = DotProductToTableIndex(reflectDirection, m_AllAmbientEnergies[i].directDir);

        directMaskEnergy += m_AllAmbientEnergies[i].direct_e * m_muTable[directMuIndex];
        reflectMaskEnergy += m_AllAmbientEnergies[i].direct_e * m_muTable[reflectMuIndex];
    }
    
    // Compute reflection energy contributions (E(R^k_j) * beta^mu(x_j * s_d) and E(R^k_j) * beta^mu(x_j * s_r) terms).
    for (int i = 0; i < kNUM_DIRECTIONS; ++i)
    {
        int directBetaMuIndex = DotProductToTableIndex(m_AllTargetEnergies[targetIndex].directDir, m_reflectDirections[i]);
        int reflectBetaMuIndex = DotProductToTableIndex(reflectDirection, m_reflectDirections[i]);

        directMaskEnergy += m_reflectEnergy[i] * m_betaMuTable[directBetaMuIndex];
        reflectMaskEnergy += m_reflectEnergy[i] * m_betaMuTable[reflectBetaMuIndex];
    }

    // Compute the signal-to-mask ratios.
    float directEnergySmr = m_AllTargetEnergies[targetIndex].direct_e / directMaskEnergy;
    float reflectEnergySmr = reflectMag / reflectMaskEnergy;
    float totalEnergySum = directEnergySmr + reflectEnergySmr;
    float recipTotalEnergySum = 1.0f / (totalEnergySum + FLT_MIN);

    // Compute weighted (percent) contributions of direct vs reflect on final decision.
    directPercent = directEnergySmr * recipTotalEnergySum;
    reflectPercent = reflectEnergySmr * recipTotalEnergySum;

    // Flip direction of reflections dir to match direct dir
    reflectDirection *= -1;
    // Interpolate directions using weighted contributions.
    direction = m_AllTargetEnergies[targetIndex].directDir * directPercent + reflectDirection * reflectPercent;
    // convert UE to Triton
    direction.Set(-direction.X, direction.Y, -direction.Z);

    // Convert total energy to dB-SMR and compute an overall confidence value using a Sigmoid function.
    float totalDbSmr = 10.0f * std::log10f(totalEnergySum);
    confidence = Sigmoid(totalDbSmr, MinMaskingThresholdDb, MaxMaskingThresholdDb);

    AddEnergy(m_AllTargetEnergies[targetIndex]);

    // I want raw SMRs, not weights
    directPercent = energyToDb(directEnergySmr);
    reflectPercent = energyToDb(reflectEnergySmr);
}

void UAcousticsSecondaryListener::AccumulateTargets()
{
    auto akd = FAkAudioDevice::Get();
    const auto listenerLoc = GetOwner()->GetActorLocation();
    for (int i = 0; i < TargetObjects.Num(); i++)
    {
        const auto a = TargetObjects[i];

        UAcousticsSecondarySource* sourceInfo = nullptr;
        UAcousticsAudioComponent* sourceAudio = nullptr;
        const auto comps = a->GetComponents();
        bool isSourceActive = false;
        for (const auto& c : comps)
        {
            if (c->IsA<UAcousticsAudioComponent>())
            {
                sourceAudio = (UAcousticsAudioComponent*)c;
                if (sourceAudio->HasActiveEvents())
                {
                    isSourceActive = true;
                }
            }
            if (c->IsA<UAcousticsSecondarySource>())
            {
                sourceInfo = (UAcousticsSecondarySource*)c;
            }
        }
        // Source is not playing. Move on.
        if (!isSourceActive)
        {
            m_AllTargetParams.Add(CreateFailedParams());
            continue;
        }

        TritonAcousticParameters tritonParams;
        const auto sourceLoc = sourceAudio->GetComponentLocation();
#if !UE_BUILD_SHIPPING
        INC_DWORD_STAT_BY(STAT_Acoustics_NpcQuery, 1);
#endif
        if (!m_Acoustics->QueryAcoustics(i, sourceLoc, listenerLoc, tritonParams))
        {
            // Bad query. Go to next object
            continue;
        }

        m_AllTargetParams.Add(tritonParams);

        // Add in any extra loudness from the source
        float extraLoudnessDb = 0.0f;

        if (sourceInfo != nullptr)
        {
            extraLoudnessDb = sourceInfo->SoundSourceLoudness;

            SourceEnergy s = TritonParamsToSourceEnergy(tritonParams, extraLoudnessDb);
            if (!IgnoreAmbiences)
            {
                AddEnergyToNoiseFloor(s);
                AddEnergy(s);
            }
            m_AllTargetEnergies.Add(s);

            tritonParams.DirectLoudnessDB += extraLoudnessDb;
        }

        // Send loudness to Wwise
        akd->SetRTPCValue(TEXT("NoiseVolume"), extraLoudnessDb, 0, a);

        m_TotalReverb += dbToAmp(tritonParams.ReflectionsLoudnessDB + extraLoudnessDb);
        m_TotalVerb_chan0 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_0 + extraLoudnessDb);
        m_TotalVerb_chan1 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_1 + extraLoudnessDb);
        m_TotalVerb_chan2 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_2 + extraLoudnessDb);
        m_TotalVerb_chan3 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_3 + extraLoudnessDb);
        m_TotalVerb_chan4 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_4 + extraLoudnessDb);
        m_TotalVerb_chan5 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_5 + extraLoudnessDb);

        // Could cache the loudness calculation, but I'm not CPU constrained yet
        if (m_LoudestTargetIndex == -1 || CalculateLoudnessDb(tritonParams) > CalculateLoudnessDb(m_AllTargetParams[m_LoudestTargetIndex]))
        {
            m_LoudestTargetIndex = i;
        }
    }
}

void UAcousticsSecondaryListener::AccumulateAmbiences()
{
    // Start with global background noise setting
    // Since there are 4 directional noise buckets, must divide energy by 4
    // Otherwise, there will be 4x as much ambient noise as we were intending
    float noise_e = dbToEnergy(NoiseFloorDb) / 4.0f;
    m_reverbNoiseEnergy[m_reflIndices[0]] += noise_e;
    m_reverbNoiseEnergy[m_reflIndices[1]] += noise_e;
    m_reverbNoiseEnergy[m_reflIndices[2]] += noise_e;
    m_reverbNoiseEnergy[m_reflIndices[3]] += noise_e;

    float noise_e6 = 0;
    if (Enable3D)
    {
        noise_e6 = dbToEnergy(NoiseFloorDb) / static_cast<float>(kNUM_DIRECTIONS);
    }
    else
    {
        noise_e6 = dbToEnergy(NoiseFloorDb) / 4.0f;
    }
    for (int i = 0; i < kNUM_DIRECTIONS; ++i)
    {
        m_reflectEnergy[i] += noise_e6;
    }

    auto akd = FAkAudioDevice::Get();
    const auto listenerLoc = GetOwner()->GetActorLocation();
    for (int i = 0; i < Ambiences.Num(); i++)
    {
        const auto a = Ambiences[i];

        UAcousticsSecondarySource* sourceInfo = nullptr;
        UAcousticsAudioComponent* sourceAudio = nullptr;
        const auto comps = a->GetComponents();
        bool isSourceActive = false;
        for (const auto& c : comps)
        {
            if (c->IsA<UAcousticsAudioComponent>())
            {
                sourceAudio = (UAcousticsAudioComponent*)c;
                if (sourceAudio->HasActiveEvents())
                {
                    isSourceActive = true;
                }
            }
            if (c->IsA<UAcousticsSecondarySource>())
            {
                sourceInfo = (UAcousticsSecondarySource*)c;
            }
        }
        // Source is not playing. Move on.
        if (!isSourceActive)
        {
            m_AllAmbientParams.Add(CreateFailedParams());
            continue;
        }

        TritonAcousticParameters tritonParams;
        const auto sourceLoc = sourceAudio->GetComponentLocation();
#if !UE_BUILD_SHIPPING
        INC_DWORD_STAT_BY(STAT_Acoustics_NpcQuery, 1);
#endif
        if (!m_Acoustics->QueryAcoustics(i + TargetObjects.Num(), sourceLoc, listenerLoc, tritonParams))
        {
            // Bad query. Go to next object
            continue;
        }
        m_AllAmbientParams.Add(tritonParams);

        // Add in any extra loudness from the source
        float extraLoudnessDb = 0.0f;
        if (sourceInfo != nullptr)
        {
            extraLoudnessDb = sourceInfo->SoundSourceLoudness;

            SourceEnergy s = TritonParamsToSourceEnergy(tritonParams, extraLoudnessDb);
            AddEnergyToNoiseFloor(s);
            AddEnergy(s);
            m_AllAmbientEnergies.Add(s);

            tritonParams.DirectLoudnessDB += extraLoudnessDb;
        }

        // Send loudness to Wwise
        akd->SetRTPCValue(TEXT("NoiseVolume"), extraLoudnessDb, 0, a);

        m_TotalBackgroundNoise += dbToAmp(CalculateLoudnessDb(tritonParams));
        m_TotalReverb += dbToAmp(tritonParams.ReflectionsLoudnessDB + extraLoudnessDb);
        m_TotalVerb_chan0 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_0 + extraLoudnessDb);
        m_TotalVerb_chan1 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_1 + extraLoudnessDb);
        m_TotalVerb_chan2 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_2 + extraLoudnessDb);
        m_TotalVerb_chan3 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_3 + extraLoudnessDb);
        m_TotalVerb_chan4 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_4 + extraLoudnessDb);
        m_TotalVerb_chan5 += dbToAmp(tritonParams.ReflLoudnessDB_Channel_5 + extraLoudnessDb);
    }
}

void UAcousticsSecondaryListener::AccumulatePolicyInputs()
{
#if !UE_BUILD_SHIPPING
    SCOPE_CYCLE_COUNTER(STAT_Acoustics_NpcPolicyInput);
#endif
    AccumulateTargets();
    if (ConsiderAmbiences)
    {
        AccumulateAmbiences();
    }
}

void UAcousticsSecondaryListener::ComputeKernels()
{
    std::array<float, kANGLE_COUNT> kernel;
    const float MaxAttenuationAt90Deg = -7;

    // Generate the source-width spreading kernel.
    kernel.fill(0);
    generateSourceSpreadKernel(kernel.data(), kANGLE_COUNT);

    // Generate the angle-dependent-attenuation masking kernel.
    kernel.fill(0);
    generateMaskingKernel(kernel.data(), kANGLE_COUNT, 0, MaxAttenuationAt90Deg);
}

void UAcousticsSecondaryListener::EvaluatePolicy()
{
    float confidence = 0;
    float max_confidence = 0;
    FVector direction;
    FVector max_direction = ((APawn*)(GetOwner()))->GetActorForwardVector();
    
    float loudest_target_e = 0;
    
    m_LoudestTargetIndex = -1;
    
    float dc, rc;
    for (size_t i = 0; i < m_AllTargetEnergies.Num(); ++i)
    {
        ComputeAudibility(i, direction, confidence, dc, rc);
        if (confidence > max_confidence)
        {
            max_confidence = confidence;
            m_DirectConfidence = dc;
            m_ReflectionsConfidence = rc;
            max_direction = direction;
            float total_e = m_AllTargetEnergies[i].direct_e + 
                m_AllTargetEnergies[i].refl_0_e + 
                m_AllTargetEnergies[i].refl_90_e + 
                m_AllTargetEnergies[i].refl_180_e + 
                m_AllTargetEnergies[i].refl_270_e;
            if (total_e > loudest_target_e)
            {
                loudest_target_e = total_e;
                m_LoudestTargetIndex = i;
            }
        }
    }

    m_Confidence = max_confidence;
    m_TargetVelocity = max_direction;
    m_CurrentWalkSpeed = WalkSpeed * max_confidence;
    m_TimeSinceLastUpdate = 0;
}

void UAcousticsSecondaryListener::ApplyPolicy()
{
    // Current implementation always has pawn moving towards loudest sound
    // Go do it.
    m_CurrentVelocity = m_TargetVelocity;// FMath::VInterpTo(m_CurrentVelocity, m_TargetVelocity, 0.15, 0.30f);
    auto own = (APawn*)(GetOwner());
    if (own != nullptr)
    {
        own->AddMovementInput(m_CurrentVelocity, m_CurrentWalkSpeed);
        if (m_CurrentVelocity.Z > 0.6f)
        {
            ((ACharacter*)own)->Jump();
        }
    }
}

void UAcousticsSecondaryListener::ShowDebugInfo()
{
    // Update debug display
    int debugSourceIndex = 0;
    for (int i = 0; i < m_AllTargetParams.Num(); i++)
    {
        m_Acoustics->UpdateSourceDebugInfo(
            debugSourceIndex,
            true,
            FName(FString::FromInt(debugSourceIndex)),
            i == m_LoudestTargetIndex,
            m_Confused);
        debugSourceIndex += 1;
    }
    for (int i = 0; i < m_AllAmbientParams.Num(); i++)
    {
        m_Acoustics->UpdateSourceDebugInfo(
            debugSourceIndex,
            true,
            FName(FString::FromInt(debugSourceIndex)),
            false,
            m_Confused);
        debugSourceIndex += 1;
    }

    m_Acoustics->UpdateConfidenceValues(m_CurrentVelocity, m_Confidence);
}

AActor* UAcousticsSecondaryListener::GetLoudestActor()
{
    if (m_LoudestTargetIndex >= 0)
    {
        return TargetObjects[m_LoudestTargetIndex];
    }
    return nullptr;
}

#if WITH_EDITOR
// React to changes in properties that are not handled in Tick()
void UAcousticsSecondaryListener::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
    Super::PostEditChangeProperty(e);
    auto world = GetWorld();
    if (world && world->IsGameWorld())
    {
        
    }
}
#endif