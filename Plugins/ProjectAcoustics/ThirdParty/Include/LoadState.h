#pragma once

namespace TritonRuntime
{
    enum class LoadState
    {
        Loaded,
        NotLoaded,
        LoadFailed,
        LoadInProgress,
        DoesNotExist,
        Invalid
    };
}