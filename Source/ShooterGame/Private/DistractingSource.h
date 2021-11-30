// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AcousticsAudioComponent.h"
#include "AcousticsSecondarySource.h"
#include "AcousticsSecondaryListener.h"
#include "DistractingSource.generated.h"

UCLASS(config = Engine, hidecategories = Auto, AutoExpandCategories = Acoustics, BlueprintType, Blueprintable,
	ClassGroup = Acoustics)
class ADistractingSource : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADistractingSource();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Acoustics, meta = (AllowPrivateAccess = "true"))
	UAcousticsAudioComponent* AudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Acoustics, meta = (AllowPrivateAccess = "true"))
	UAcousticsSecondarySource* SecondarySource;

	UFUNCTION(BlueprintCallable, Category = "Acoustics")
	void SetAcousticsListener(UAcousticsSecondaryListener* listener) {m_listener = listener;}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	UAcousticsSecondaryListener* m_listener;

};
