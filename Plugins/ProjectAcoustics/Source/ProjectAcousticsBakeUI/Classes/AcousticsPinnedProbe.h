// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "GameFramework/Volume.h"
#include "AcousticsPinnedProbe.generated.h"

UCLASS(ClassGroup = ProjectAcoustics, hidecategories = (Advanced, Attachment), BlueprintType)
class AAcousticsPinnedProbe : public AActor
{
    GENERATED_UCLASS_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AcousticsPinnedProbe")
    class UStaticMeshComponent* ProbeMesh;

public:
    // This class only helps with Acoustics pre-bake design, and is not meant for use in-game.
    virtual bool IsEditorOnly() const override
    {
        return true;
    }
};