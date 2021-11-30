// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

namespace Triton
{
#pragma pack(push, 4)
    template <typename T>
    union Vector3 {
        struct
        {
            T x;
            T y;
            T z;
        };
        T arr[3];
    };
#pragma pack(pop)

    using Vec3i = Vector3<int>;
    using Vec3f = Vector3<float>;
} // namespace Triton
