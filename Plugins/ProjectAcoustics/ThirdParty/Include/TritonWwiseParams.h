// Copyright (c) Microsoft Corporation.  All rights reserved.
//! \file TritonWwiseParams.h
//! \brief Types to be used with Triton when integrated with AudioKinetic's Wwise audio engine.
#pragma once

#include "TritonApiTypes.h"
#include <cstdint>
#include <cmath>

#pragma pack(push, 4)

//! Struct that captures designer input used to modify rendered acoustics
struct UserDesign
{
public:
    //! OcclusionMultiplier: Apply a multiplier to the occlusion dB level computed by the acoustics
    //! system. If this multiplier is greater than 1, occlusion will be exaggerated, while values less than 1 make
    //! the occlusion effect more subtle, and a value of 0 disables occlusion.
    float OcclusionMultiplier;

    //! WetnessAdjustment: Adjusts the reverb power, in dB, by the given offset from the value
    //! computed by the acoustics system. Positive values make a sound more reverberant, while negative values make a
    //! sound more dry.
    float WetnessAdjustment;

    //!  DecayTimeMultiplier: Applies a multiplier to the reverb decay time. For example, if the bake
    //! result specifies a decay time of 750 milliseconds, but this value is set to 1.5, the decay time applied to
    //! the source is 1,125 milliseconds.
    float DecayTimeMultiplier;

    //! OutdoornessAdjustment: An additive adjustment on the acoustics system’s estimate of how
    //! "outdoors" the reverberation on a source should sound. Setting this to 1 will make a source always sound
    //! completely outdoors, while setting it to -1 will make it always sound indoors.
    float OutdoornessAdjustment;

    //! TranmissionDb: Specify additional dry signal propagated in straight line from source to listener through
    //! geometry, in dB.
    float TransmissionDb;

    //! DRR adjustment: Changes perceived distance by manipulating wet-dry ratio only.
    //! Values smaller than 0 makes the source sound drier and more intimate,
    //! larger than zero makes a sound more aggressively reverberant as it moves
    //! away from the listener.
    float DRRDistanceWarp;

    // Static Methods

    //! Minimum possible values for designer input parameters
    //! \return Minimum possible values for designer input parameters
    static const UserDesign& Min()
    {
        static UserDesign v{0.0f, -20.0f, 0.0f, -1.0f, -60.0f, -1.0f};
        return v;
    }

    //! Default values for designer input parameters
    //! \return Default values for designer input parameters
    static const UserDesign& Default()
    {
        static UserDesign v{1.0f, 0.0f, 1.0f, 0.0f, -60.0f, 0.0f};
        return v;
    }

    //! Maximum possible values for design input parameters
    //! \return Maximum possible values for design input parameters
    static const UserDesign& Max()
    {
        static UserDesign v{2.0f, 20.0f, 2.0f, 1.0f, 0.0f, 1.0f};
        return v;
    }

    //! Clamp all members to be within valid range
    //! \param t Object whose members will be clamped to valid range
    static void ClampToRange(UserDesign& t)
    {
        auto low = Min();
        auto high = Max();
        t.OcclusionMultiplier = Clamp(t.OcclusionMultiplier, low.OcclusionMultiplier, high.OcclusionMultiplier);
        t.WetnessAdjustment = Clamp(t.WetnessAdjustment, low.WetnessAdjustment, high.WetnessAdjustment);
        t.DecayTimeMultiplier = Clamp(t.DecayTimeMultiplier, low.DecayTimeMultiplier, high.DecayTimeMultiplier);
        t.OutdoornessAdjustment = Clamp(t.OutdoornessAdjustment, low.OutdoornessAdjustment, high.OutdoornessAdjustment);
        t.TransmissionDb = Clamp(t.TransmissionDb, low.TransmissionDb, high.TransmissionDb);
        t.DRRDistanceWarp = Clamp(t.DRRDistanceWarp, low.DRRDistanceWarp, high.DRRDistanceWarp);
    }

    //! Combine two sets of design values. "base" is modified to incorporate "other".
    //! \param base Object whose members will be modified
    //! \param other Object whose values are incorporated
    static void Combine(UserDesign& base, const UserDesign& other)
    {
        base.OcclusionMultiplier *= other.OcclusionMultiplier;
        base.WetnessAdjustment += other.WetnessAdjustment;
        base.DecayTimeMultiplier *= other.DecayTimeMultiplier;
        base.OutdoornessAdjustment += other.OutdoornessAdjustment;
        base.TransmissionDb = EnergyToDb(DbToEnergy(base.TransmissionDb) + DbToEnergy(other.TransmissionDb));
        base.DRRDistanceWarp += other.DRRDistanceWarp;
    }

private:
    //! Clamp value to be between min amd max.
    //! \param v value to be clamped
    //! \param vMin Lower boundary
    //! \param vMax Upper boundary
    //! \return Clamped value
    static inline float Clamp(float v, float vMin, float vMax)
    {
        return v < vMin ? vMin : (v > vMax ? vMax : v);
    }

    //! Convert from dB to Energy
    //! \param DB value in dB
    //! \return energy value
    static float DbToEnergy(float dB)
    {
        return std::pow(10.0f, (dB / 10.0f));
    }

    //! Convert from energy to dB
    //! \param energy value in energy
    //! \return dB value
    static float EnergyToDb(float energy)
    {
        return 10.0f * std::log10(energy);
    }
};
#pragma pack(pop)

#pragma pack(push, 4)
//! Holds perceptual acoustic parameters and design tweaks for a particular Wwise game object.
//! For more information, see the documentation for TritonAcousticParameters
struct TritonWwiseParams
{
    //! The ID that Wwise uses to keep track of the game object
    uint64_t ObjectId;
    //! The TritonAcousticParameters for this voice
    TritonAcousticParameters TritonParams;
    //! The outdoorness for this voice at the current listener location. 0 = completely indoors, 1 = completely outdoors
    float Outdoorness;
    //! Per-voice design tweaks
    UserDesign Design;
};
#pragma pack(pop)