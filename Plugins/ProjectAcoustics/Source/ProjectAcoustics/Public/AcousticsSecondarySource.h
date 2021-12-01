// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include "Components/SceneComponent.h"
#include "IAcoustics.h"
#include "AcousticsSecondarySource.generated.h"

UCLASS(
    hidecategories = Auto, AutoExpandCategories = (Acoustics), BlueprintType, Blueprintable,
    ClassGroup = Acoustics, meta = (BlueprintSpawnableComponent))
class PROJECTACOUSTICS_API UAcousticsSecondarySource : public USceneComponent
{
    GENERATED_BODY()
public:
    // Value is in dB
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustics")
    float SoundSourceLoudness = 0.0f;
//
//public:
//    
//#if CPP
//    virtual void BeginPlay() override;
//    virtual void
//    TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
//    virtual void OnUnregister() override;
//#endif
};
