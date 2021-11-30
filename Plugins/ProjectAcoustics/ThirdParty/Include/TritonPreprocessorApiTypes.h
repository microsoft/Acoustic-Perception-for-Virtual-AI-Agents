// Copyright (c) Microsoft Corporation.  All rights reserved.
//! \file TritonPreprocessorApiTypes.h
#pragma once
#include "AcousticsSharedTypes.h"

//! Handle to an acoustics object
typedef OBJECT_HANDLE TritonObject;

//! Maximum string length for a name field
#define TRITON_MAX_NAME_LENGTH 128

//! Maximum path length
#define TRITON_MAX_PATH_LENGTH 255

//! Default acoustics material code
#define TRITON_DEFAULT_WALL_CODE 2

//! Each material is assigned a unique 64-bit integer
//! This typedef helps differentiate material codes from other integer types
typedef long long TritonMaterialCode;
static_assert(sizeof(long long) == 8, "Triton material codes are signed 64-bit ints");

//! Indicates the role of a provided object mesh
enum MeshType
{
    //! Indicates acoustic geometry that interacts with sound waves
    MeshTypeGeometry = 0,
    //! Indicates areas player can navigate to
    MeshTypeNavigation = 1,
    //! Indicates a watertight volume that restricts player probe sampling to its interior
    MeshTypeIncludeVolume = 2,
    //! Indicates a watertight volume such that no player probes are placed in its interior
    MeshTypeExcludeVolume = 3,
    //! Indicates a watertight volume part of which will be turned into solid during voxelization using a flooding seed
    //! location
    MeshTypeGeometryFillVolume = 4,
    //! Indicates a watertight volume such that probe layout inside this volume adheres to specified probe resolution
    MeshTypeProbeSpacingVolume = 5,
    //! Invalid type
    MeshTypeInvalid
};

//! Bounding box structure used to define
//! simulation regions
typedef struct TritonBoundingBox
{
    //! Minimum corner
    ATKVectorF MinCorner;
    //! Maximunm corner
    ATKVectorF MaxCorner;
} TritonBoundingBox;

//! Settings used to control simulation probe layouot
typedef struct TritonProbeSamplingSpecification
{
    //! Minimum horizontal distance between probes
    float MinHorizontalSpacing;
    //! Maximum horizontal distance between probes
    float MaxHorizontalSpacing;
    //! Vertical distance separating brobes
    float VerticalSpacing;
    //! Minimal distance from the ground at which probes should be placed
    float MinHeightAboveGround;
} TritonProbeSamplingSpecification;

//! Defines bounding boxes used for probe simulation regions
typedef struct TritonSimulationRegionSpecification
{
    //! Inner bounding box for per probe simulation region
    TritonBoundingBox Small;
    //! Outer bounding box for per probe simulation region
    TritonBoundingBox Large;
} TritonSimulationRegionSpecification;

//! Specifies the constraints for the simulation
typedef struct TritonSimulationParameters
{
    //! Scalar to apply to the mesh to convert to Triton's units (meters)
    float MeshUnitAdjustment;
    //! Scalar to apply to the scene for appropriate scaling
    float SceneScale;
    //! Speed of sound (in m/s) used for simulation
    float SpeedOfSound;
    //! Probing frequency used for simulation
    float SimulationFrequency;
    //! Spatial sampling resolution for runtime sound sources
    float ReceiverSampleSpacing;
    //! Resolution of voxel map used during runtime for interpolation
    float VoxelMapResolution;
    //! Configuration to control placement of simulation probes
    TritonProbeSamplingSpecification ProbeSpacing;
    //! Configuration to control simulation region around a probe
    TritonSimulationRegionSpecification PerProbeSimulationRegion;
} TritonSimulationParameters;

//! Specifies metadata parameters to use while performing the simulation
typedef struct TritonOperationalParameters
{
    //! Prefix used for files created during processing
    char Prefix[TRITON_MAX_NAME_LENGTH];
    //! Path to the working directory for processing
    char WorkingDir[TRITON_MAX_PATH_LENGTH];
    //! File containing acoustic materials information
    char MaterialFilename[TRITON_MAX_PATH_LENGTH];
    //! FBX file with the geometry to be processed
    char MeshFilename[TRITON_MAX_PATH_LENGTH];
    //! File with job configuration settings
    char JobFilename[TRITON_MAX_PATH_LENGTH];
    //! Set to true to optimize processed voxel map
    bool OptimizeVoxelMap;
    //! Set to true to disable PML processing
    bool DisablePml;
} TritonOperationalParameters;

//! Describes a triangle used in the simulation
typedef struct TritonAcousticMeshTriangleInformation
{
    //! Indices into a vertex buffer for this triangle
    ATKVectorI Indices;
    //! Acoustic material code applied to this triangle
    TritonMaterialCode MaterialCode;
} TritonAcousticMeshTriangleInformation;

//! Describes a material used in the simulation
typedef struct TritonAcousticMaterial
{
    //! Name of the acoustic material
    char Name[TRITON_MAX_NAME_LENGTH];
    //! Absorptivity coefficient for the material
    float Absorptivity;
} TritonAcousticMaterial;
