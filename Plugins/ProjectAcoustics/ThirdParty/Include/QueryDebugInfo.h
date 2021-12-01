// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "MemoryOverrides.h"
#include "ProbeInterp.h"
#include "ReceiverInterpolationWeights.h"

namespace TritonRuntime
{
    struct DynamicOpeningDebugInfo
    {
        TRITON_PREVENT_HEAP_ALLOCATION;

    public:
        bool DidGoThroughOpening;
        bool DidProcessingSucceed;
        uint64_t OpeningID;
        Triton::Vec3f Center;
        int BoundProbeID;
        Triton::Vec3f StringTightenedPoint;
        float DistanceDifference;
    };

    class QueryDebugInfo
    {
        friend class DecodedProbeData;
        friend class SpatialProbeDistribution;
        friend class EncodedMapData;
        friend class DebugInfoWeightSetter;

        TRITON_PREVENT_HEAP_ALLOCATION;

    public:
        static const int MaxMessages = 16;

        enum MessageType
        {
            NoError,
            Warning,
            Error,
            Fatal
        };

        struct DebugMessage
        {
            TRITON_PREVENT_HEAP_ALLOCATION;

        public:
            static const int MaxMessageLength = 128;

            MessageType Type;
            wchar_t MessageString[MaxMessageLength];

            inline void ResetType()
            {
                Type = NoError;
            }
        };

    private:
        DebugMessage _Messages[MaxMessages];
        int _MessageCount;
        bool _DidQuerySucceed;

        ProbeInterpVals _ProbeWeights[MaxInterpProbes];
        ReceiverInterpolationWeights _ReceiverWeights[MaxInterpProbes];

        bool _DidConsiderDynamicOpenings;
        DynamicOpeningDebugInfo _DynamicOpeningInfo;

        bool _PushMessage(MessageType type, const wchar_t* Message);
        void _Reset();

        void _SetProbeWeights(const ProbeInterpVals* vals);
        void _SetReceiverWeights(int ProbeIndex, const ReceiverInterpolationWeights& ReceiverWeights);

    public:
        QueryDebugInfo();
        ~QueryDebugInfo();

        int CountMessagesOfType(MessageType type) const;

        void GetProbeInterpWeights(ProbeInterpVals OutWeights[MaxInterpProbes]) const;
        ReceiverInterpolationWeights GetReceiverInterpWeightsForProbe(int ProbeIndex) const;
        int GetMessageCount() const;
        bool DidQuerySucceed() const;
        const DebugMessage* GetMessageList(int& OutNumMessages) const;

        bool DidConsiderDynamicOpenings() const;
        const DynamicOpeningDebugInfo* GetDynamicOpeningDebugInfo() const;
    };
} // namespace TritonRuntime