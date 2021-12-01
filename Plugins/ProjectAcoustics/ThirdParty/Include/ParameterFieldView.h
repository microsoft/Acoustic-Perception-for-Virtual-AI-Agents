#pragma once
#include "AcousticParameterIds.h"
#include "MemoryOverrides.h"
#include "TritonVector.h"

namespace TritonRuntime
{
    struct ParameterFieldViewImpl;
    class DebugDataView;

    /// <summary>
    /// 		  Represents the acoustic parameter fields for a probe.
    /// </summary>
    class ParameterFieldView
    {
        DEFINE_TRITON_CUSTOM_ALLOCATORS
        friend class DebugDataView;

    private:
        DebugDataView* _DebugView;
        ParameterFieldViewImpl* _Impl;
        ParameterID _LastRead;

        ParameterFieldView(DebugDataView* DebugView, ParameterFieldViewImpl* Impl);

        bool _HasBeenReleased();
        void _Release();

        // Disallow copying.
        ParameterFieldView(const ParameterFieldView&);
        ParameterFieldView& operator=(const ParameterFieldView&);
        virtual ~ParameterFieldView();

    public:
        /// <summary>	Deallocates the view, releasing resources. </summary>
        ///
        /// <param name="v">	[in,out] If non-null, the view to destroy. </param>

        static void Destroy(const ParameterFieldView* v);

        /// <summary>	Gets the 3D field resolution. </summary>
        /// <returns>	The field dimensions. </returns>

        Triton::Vec3i GetFieldDimensions() const;

        /// <summary>	Gets the min corner of the bounding box of
        /// 			the field data in mesh coordinates
        /// </summary>
        ///
        /// <returns>	The minimum corner. </returns>

        Triton::Vec3f GetMinCorner() const;

        /// <summary>	Gets the increment vector to go from entry (x,y,z)
        /// 			in the field array to (x+1,y+1,z+1).
        /// </summary>
        ///
        /// <returns>	The cell increment. </returns>

        Triton::Vec3f GetCellIncrement() const;

        // Find length of OutField array, in bytes, to hold field volume data
        int GetFieldVolumeSize(ParameterID Param) const;

        /// <summary>	Gets field volume. </summary>
        ///
        /// <param name="Param">   	The acoustic parameter. </param>
        /// <param name="OutField">	[in,out] If non-null, the output field.
        /// 						The array's size must be at least GetFieldVolumeSize().
        /// 						The output data is ordered so x increases first.
        /// </param>
        ///
        /// <returns>	true if it succeeds, false if it fails. </returns>

        bool GetFieldVolume(ParameterID Param, char* OutField) const;

        /// <summary>	Reads field volume at a specified voxel (x,y,z).
        /// 			Each coordinate must be in [0,nx-1] inclusive and so on.
        /// 			These dimensions can be obtained from GetFieldDimensions().
        /// </summary>
        ///
        /// <param name="Field">	Must be non-null, the field obtained from GetFieldVolume() </param>
        /// <param name="x">		The x coordinate. </param>
        /// <param name="y">		The y coordinate. </param>
        /// <param name="z">		The z coordinate. </param>
        ///
        /// <returns>	The field volume. </returns>

        float ReadScalarFieldVolume(
            ParameterID Param, const char* Field, unsigned int x, unsigned int y, unsigned int z) const;

        // Find length of OutField array, in bytes, to be used in call to GetFieldSlice
        int GetFieldSliceSize(ParameterID Param) const;

        /// <summary>	Gets field slice. </summary>
        ///
        /// <param name="Param">   	The acoustic parameter. </param>
        /// <param name="SliceZ">  	The slice z coordinate. </param>
        /// <param name="OutField">	[in,out] If non-null, the output field slice data.
        /// 						The array's size must be at least GetFieldSliceSize().
        /// 						The output data is ordered so x increases first.
        /// </param>
        ///
        /// <returns>	true if it succeeds, false if it fails. </returns>

        bool GetFieldSlice(ParameterID Param, int SliceZ, char* OutField) const;

        /// <summary>	Reads field slice at specified pixel (x,y).
        /// 			Each coordinate must be in [0,nx-1] inclusive and so on.
        /// 			These dimensions can be obtained from GetFieldDimensions().
        /// </summary>
        ///
        /// <param name="Field">	Must be non-null, the field obtained from GetFieldSlice(). </param>
        /// <param name="x">		The x coordinate. </param>
        /// <param name="y">		The y coordinate. </param>
        ///
        /// <returns>	The field slice. </returns>

        float ReadScalarFieldSlice(ParameterID Param, const char* Field, unsigned int x, unsigned int y) const;
    };
} // namespace TritonRuntime