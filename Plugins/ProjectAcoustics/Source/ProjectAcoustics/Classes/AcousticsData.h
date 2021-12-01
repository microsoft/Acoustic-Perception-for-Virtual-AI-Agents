// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include "Engine/GameEngine.h"
#include "AcousticsData.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class PROJECTACOUSTICS_API UAcousticsData : public UObject
{
    GENERATED_UCLASS_BODY()

public:
    /** Relative path to the ACE file. The actual ACE file must be manually placed at this location
     *   separate from this uasset, otherwise it may not be packaged as part of the game and the Project
     *   Acoustics runtime will not be able to find it. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Acoustics")
    FString AceFilePath;

    virtual void PostRename(UObject* OldOuter, const FName OldName) override;

private:
    void UpdateAceFilePath();
};