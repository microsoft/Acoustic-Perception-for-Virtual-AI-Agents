// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPCAICharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "AkComponent.h"

//////////////////////////////////////////////////////////////////////////
// ANPCAICharacter

ANPCAICharacter::ANPCAICharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->SocketOffset.Z = 100.0f;
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	AcousticsListener = CreateDefaultSubobject<UAcousticsSecondaryListener>(TEXT("AcousticsListener"));
	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	AkListener = CreateDefaultSubobject<UAkComponent>(TEXT("AkListener"));
	AkListener->SetupAttachment(RootComponent);

	NpcGunSound = CreateDefaultSubobject<UAcousticsAudioComponent>(TEXT("NPCGunSound"));
	NpcGunSound->SetupAttachment(RootComponent);
}

const FString c_AuxBusNames[6] = { TEXT("Verb_X_Minus"),
								TEXT("Verb_X_Plus"),
								TEXT("Verb_Y_Minus"),
								TEXT("Verb_Y_Plus"),
								TEXT("Verb_Z_Minus"),
								TEXT("Verb_Z_Plus") };
void ANPCAICharacter::BeginPlay()
{
	Super::BeginPlay();
	AcousticsListener->RegisterComponent();
	
	auto akd = FAkAudioDevice::Get();
	auto ls = akd->GetDefaultListeners();
	auto first = ls.begin();
	akd->RemoveDefaultListener(*first);
	
	akd->RegisterComponent(AkListener);
	akd->AddDefaultListener(AkListener);
	akd->SetSpatialAudioListener(AkListener);
	
	akd->LoadInitBank();
	akd->LoadAllReferencedBanks();
	
	
}

void ANPCAICharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AcousticsListener->UnregisterComponent();

	Super::EndPlay(EndPlayReason);
}


void ANPCAICharacter::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	// Must update our own listener to get things rotated the right way round
	const auto actorLoc = GetPawnViewLocation();
	AkSoundPosition pos;
	AkVector akPos;
	akPos.X = actorLoc.X;
	akPos.Y = actorLoc.Y;
	akPos.Z = actorLoc.Z;
	pos.SetPosition(akPos);

	const auto fwd = GetActorForwardVector();
	AkVector akFwd;
	akFwd.X = fwd.X;
	akFwd.Y = fwd.Y;
	akFwd.Z = fwd.Z;

	const auto up = GetActorUpVector();
	AkVector akUp;
	akUp.X = up.X;
	akUp.Y = up.Y;
	akUp.Z = up.Z;
	pos.SetOrientation(akFwd, akUp);
	
	auto akd = FAkAudioDevice::Get();
	akd->SetPosition(AkListener, pos);
}

//////////////////////////////////////////////////////////////////////////
// Input

void ANPCAICharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ANPCAICharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ANPCAICharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ANPCAICharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ANPCAICharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ANPCAICharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ANPCAICharacter::TouchStopped);
}


void ANPCAICharacter::OnResetVR()
{
}

void ANPCAICharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void ANPCAICharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void ANPCAICharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ANPCAICharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ANPCAICharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ANPCAICharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
