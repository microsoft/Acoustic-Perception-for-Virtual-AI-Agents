// Copyright (c) Microsoft Corporation.  All rights reserved.

#pragma once
#include "MemoryOverrides.h"
#include "TritonVector.h"

namespace TritonRuntime
{
    class MapCompressedBinary;

    /// <summary>	Represents an axis-aligned box section of Triton's voxel representation of the scene </summary>
    class VoxelmapSection
    {
        DEFINE_TRITON_CUSTOM_ALLOCATORS;
        friend class DebugDataView;

    private:
        DebugDataView* _DebugView;
        MapCompressedBinary* _Map;
        Triton::Vec3f _MinCorner;
        Triton::Vec3i _NumCells;
        Triton::Vec3f _CellIncrementVector;
        Triton::Vec3i _MinCornerCoarseIndex;
        int _Refinement;

        bool _HasBeenReleased();
        void _Release();

        VoxelmapSection(
            DebugDataView* DebugView, MapCompressedBinary* VoxMap, Triton::Vec3i NumCells,
            Triton::Vec3i MinCornerCoarseIndex, int Refinement, Triton::Vec3f MinCorner,
            Triton::Vec3f CellIncrementVector);
        virtual ~VoxelmapSection();

    public:
        /// <summary>	Deallocates the view, releasing resources. </summary>
        ///
        /// <param name="v">	[in,out] If non-null, the view to destroy. </param>
        static void Destroy(const VoxelmapSection* v);

        // Number of cells in each dimension in 3D voxel array
        Triton::Vec3i GetNumCells() const;

        // Access cell in 3D voxel array and find if its not air
        bool IsVoxelWall(int x, int y, int z) const;

        // Get minimum corner of the voxel section in mesh coordinates
        // (same as used by all Triton functions)
        Triton::Vec3f GetMinCorner() const;

        // This is the amount we move in space when moving from
        // the center of any voxel accessed in the 3D array at
        // (x,y,z) to the diagonal voxel at (x+1,y+1,z+1)
        //
        // Combined with GetMinCorner() this provides all info
        // needed to draw the voxels correctly registered to the scene
        Triton::Vec3f GetCellIncrementVector() const;
    };
} // namespace TritonRuntime