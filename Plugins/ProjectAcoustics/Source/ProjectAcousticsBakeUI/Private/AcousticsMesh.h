// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "TritonPreprocessorApi.h"
// Add include for non-unity build
#include "Templates/UniquePtr.h"

// C++ wrappers for Triton Preprocessor types
class AcousticMesh final
{
public:
    ~AcousticMesh();
    static TUniquePtr<AcousticMesh> Create();
    bool
    Add(ATKVectorF* vertices, int vertexCount, TritonAcousticMeshTriangleInformation* triangleInfos, int trianglesCount,
        MeshType type);
    bool AddProbeSpacingVolume(
        ATKVectorF* vertices, int vertexCount, TritonAcousticMeshTriangleInformation* triangleInfos, int trianglesCount,
        float spacing);
    bool AddPinnedProbe(ATKVectorF probeLocation);
    const TritonObject& GetHandle() const;
    bool HasNavigationMesh() const
    {
        return m_HasNavigationMesh;
    }

private:
    AcousticMesh() : m_Handle(nullptr), m_HasNavigationMesh(false)
    {
    }

    bool Initialize();

private:
    TritonObject m_Handle;
    bool m_HasNavigationMesh;
};
