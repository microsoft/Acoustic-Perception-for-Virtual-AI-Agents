// Fill out your copyright notice in the Description page of Project Settings.


#include "DistractingSource.h"

// Sets default values
ADistractingSource::ADistractingSource()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AudioComponent = CreateDefaultSubobject<UAcousticsAudioComponent>(TEXT("AcousticsAudio"));
	AudioComponent->SetupAttachment(RootComponent);

	SecondarySource = CreateDefaultSubobject<UAcousticsSecondarySource>(TEXT("AcousticsSecondarySource"));
	SecondarySource->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ADistractingSource::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ADistractingSource::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!AudioComponent->HasActiveEvents())
	{
		//m_listener->DangerObjects.Remove(this);
		Destroy();
	}
}

