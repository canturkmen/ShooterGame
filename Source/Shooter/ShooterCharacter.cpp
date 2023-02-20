// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterCharacter.h"

#include "Item.h"
#include "Weapon.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "Components/WidgetComponent.h"

// Sets default values
AShooterCharacter::AShooterCharacter()
	: BaseTurnRate(45.0f), BaseLookUpRate(45.0f), 
	// Turn / Lookup Rate Configurations when aiming and hip firing
	HipTurnRate(90.0f),
	HipLookUpRate(90.0f),
	AimingTurnRate(20.0f),
	AimingLookUpRate(20.0f),
	// Turn / Lookup Rate Configurations when aiming and hip firing with mouse
	MouseHipTurnRate(1.0f),
	MouseHipLookUpRate(1.0f),
	MouseAimingTurnRate(0.2f),
	MouseAimingLookUpRate(0.2f),
	// Set to true when aiming
	bAiming(false), 
	// Camera FOV Configurations
	CameraDefaultFov(0.0f), CameraZoomedFov(35.0f), CameraCurrentFov(0.0f), ZoomInterpSpeed(20.0f),
	// Crosshair Spread Factors
	CrosshairSpreadMultiplier(0.0f),
	CrosshairVelocityFactor(0.0f),
	CrosshairInAirFactor(0.0f),
	CrosshairAimFactor(0.0f),
	CrosshairShootingFactor(0.0f),
	// Bullet fired timer
	ShootTimeDuration(0.05f),
	bFiringBullet(false),
	// Automatic fire variables
	FiringRate(0.1f),
	bShouldFire(true),
	bFireButtonPressed(false),
	bShouldTraceForItem(false),
	CameraInterpDistance(250.0f),
	CameraInterpElevation(65.0f)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom (pulls in towards the character if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 180.0f; // Camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	CameraBoom->SocketOffset = FVector(0.0f, 50.0f, 70.0f);

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach camera to the end of spring arm
	FollowCamera->bUsePawnControlRotation = false; // Camera doesn't rotate relative to the arm)

	// Don't rotate when the controller rotates. Let controller only affect the camera
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // Character rotates at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.0f;
	GetCharacterMovement()->AirControl = 0.2f;
}

void AShooterCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && Value != 0)
	{
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation = { 0.0f, Rotation.Yaw, 0.0f };

		const FVector Direction = FRotationMatrix{ YawRotation }.GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && Value != 0)
	{
		// Find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation = { 0.0f, Rotation.Yaw, 0.0f };

		const FVector Direction = FRotationMatrix{ YawRotation }.GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::TurnAtRate(float Rate)
{
	// Calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate* GetWorld()->GetDeltaSeconds()); // (deg / sec) * (sec / frame) = (deg / frame) 
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	// Calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds()); // (deg / sec) * (sec / frame) = (deg / frame) 
}

void AShooterCharacter::TurnWithMouse(float Value)
{
	float TurnScaleFactor;
	if (bAiming)
		TurnScaleFactor = MouseAimingTurnRate;
	else
		TurnScaleFactor = MouseHipTurnRate;

	AddControllerYawInput(Value * TurnScaleFactor);
}

void AShooterCharacter::LookUpWithMouse(float Value)
{
	float LookUpScaleFactor;
	if (bAiming)
		LookUpScaleFactor = MouseAimingLookUpRate;
	else
		LookUpScaleFactor = MouseHipLookUpRate;

	AddControllerPitchInput(Value * LookUpScaleFactor);
}

void AShooterCharacter::FireWeapon()
{
	if (FireSound)
		UGameplayStatics::PlaySound2D(this, FireSound);

	const USkeletalMeshSocket* BarrelSocket = GetMesh()->GetSocketByName("BarrelSocket");
	if (BarrelSocket)
	{
		FTransform BarrelSocketTransform = BarrelSocket->GetSocketTransform(GetMesh());
		if (MuzzleFlash)
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, BarrelSocketTransform);	

		FVector BeamEndLocation;
		bool bBeamEnd = bGetBeamEndLocation(BarrelSocketTransform, BeamEndLocation);
		  
		if (bBeamEnd)
		{
			// Spawn impact particles after updating BeamEndPoint
			if (ImpactParticles)
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, BeamEndLocation);

			if (BeamParticles)
			{
				UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BeamParticles, BarrelSocketTransform);
				if (Beam)
					Beam->SetVectorParameter(FName("Target"), BeamEndLocation);
			}
		}
	}

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HipFireMontage)
	{
		AnimInstance->Montage_Play(HipFireMontage);
		AnimInstance->Montage_JumpToSection(FName("StartFire"));
	}

	// Start bullet fire timer for crosshairs
	StartCrosshairBulletFire();
}

bool AShooterCharacter::TraceUnderCrosshairs(FHitResult& OutResult, FVector& OutLocation)
{
	// Get the Viewport Size	
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
		GEngine->GameViewport->GetViewportSize(ViewportSize);

	FVector2D CrosshairLocation(ViewportSize.X / 2.0f, ViewportSize.Y / 2.0f);

	FVector CrosshairWorldLocation;
	FVector CrosshairWorldDirection;

	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(UGameplayStatics::GetPlayerController(this, 0), CrosshairLocation, CrosshairWorldLocation, CrosshairWorldDirection);

	if (bScreenToWorld)
	{
		// Trace from Crosshair world location outward
		FVector Start = CrosshairWorldLocation;
		FVector End = Start + (CrosshairWorldDirection * 50000.0f);
		OutLocation = End;

		GetWorld()->LineTraceSingleByChannel(OutResult, Start, End, ECollisionChannel::ECC_Visibility);

		if (OutResult.bBlockingHit)	
			return true;
	}

	return false;
}

void AShooterCharacter::TraceForItems()
{
	if (bShouldTraceForItem)
	{
		FHitResult ItemTraceResult;
		FVector HitLocation;
		if (TraceUnderCrosshairs(ItemTraceResult, HitLocation))
		{
			TraceHitItem = Cast<AItem>(ItemTraceResult.GetActor());
			if (TraceHitItem && TraceHitItem->GetPickupWidget())
				TraceHitItem->GetPickupWidget()->SetVisibility(true);

			if (TraceHitItemLastFrame)
			{
				// We are hitting a different item this frame from last frame
				// Thus, we need to hide the widget
				if (TraceHitItem != TraceHitItemLastFrame)
					TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
				
			}

			// Store a reference to hit item for next frame
			TraceHitItemLastFrame = TraceHitItem;
		}
	}
	else if (TraceHitItemLastFrame)
	{
		// No longer overlapping any items. 
		// Item last frame should not show widget
		TraceHitItemLastFrame->GetPickupWidget()->SetVisibility(false);
	}
}

AWeapon* AShooterCharacter::SpawnDefaultWeapon()
{
	if (DefaultWeaponClass)
		return GetWorld()->SpawnActor<AWeapon>(DefaultWeaponClass);

	return nullptr;
}

void AShooterCharacter::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip)
	{
		const USkeletalMeshSocket* HandSocket = GetMesh()->GetSocketByName(FName("RightHandSocket"));
		if (HandSocket)
			HandSocket->AttachActor(WeaponToEquip, GetMesh());

		EquippedWeapon = WeaponToEquip;
		EquippedWeapon->SetItemState(EItemState::EIS_Equipped);
	}
}

void AShooterCharacter::DropWeapon()
{
	if (EquippedWeapon)
	{
		FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, true);
		EquippedWeapon->GetItemMesh()->DetachFromComponent(DetachmentTransformRules);
		EquippedWeapon->SetItemState(EItemState::EIS_Falling);
		EquippedWeapon->ThrowWeapon();
	}
}

void AShooterCharacter::SelectButtonPressed()
{
	if (TraceHitItem)
		TraceHitItem->StartItemInterping(this);
}

void AShooterCharacter::SelectButtonReleased()
{

}

void AShooterCharacter::SwapWeapon(AWeapon* WeaponToSwap)
{
	DropWeapon();
	EquipWeapon(WeaponToSwap);
	TraceHitItem = nullptr;
	TraceHitItemLastFrame = nullptr;
}

bool AShooterCharacter::bGetBeamEndLocation(const FTransform& MuzzelSocketLocation, FVector& OutBeamLocation)
{
	FHitResult CrosshairHitResult;
	bool bCrooshairHit = TraceUnderCrosshairs(CrosshairHitResult, OutBeamLocation);

	if (bCrooshairHit)
		OutBeamLocation = CrosshairHitResult.Location;

	// Perform a second trace, this time from the barrel
	FHitResult WeaponTraceHit;
	const FVector WeaponTraceStart = MuzzelSocketLocation.GetLocation();
	const FVector EndToStart = OutBeamLocation - WeaponTraceStart;
	const FVector WeaponTraceEnd = WeaponTraceStart + (EndToStart * 1.25f);

	GetWorld()->LineTraceSingleByChannel(WeaponTraceHit, WeaponTraceStart, WeaponTraceEnd, ECollisionChannel::ECC_Visibility);

	// Object between barrel and beam end point.
	if (WeaponTraceHit.bBlockingHit)
	{
		OutBeamLocation = WeaponTraceHit.Location;
		return true;
	}

	return false;
}

void AShooterCharacter::AimingButtonPressed()
{
	bAiming = true;
}

void AShooterCharacter::AimingButtonReleased()
{
	bAiming = false;
}

void AShooterCharacter::CameraInterpZoom(float DeltaTime)
{
	// Aiming button pressed?
	if (bAiming)
		CameraCurrentFov = FMath::FInterpTo(CameraCurrentFov, CameraZoomedFov, DeltaTime, ZoomInterpSpeed);
	else
		CameraCurrentFov = FMath::FInterpTo(CameraCurrentFov, CameraDefaultFov, DeltaTime, ZoomInterpSpeed);

	GetFollowCamera()->SetFieldOfView(CameraCurrentFov);
}

void AShooterCharacter::SetBaseTurnAndLookupRate()
{
	if (bAiming)
	{
		BaseTurnRate = AimingTurnRate;
		BaseLookUpRate = AimingLookUpRate;
	}
	else
	{
		BaseTurnRate = HipTurnRate;
		BaseLookUpRate = HipLookUpRate;
	}

}

void AShooterCharacter::CalculateCrosshairSpread(float DeltaTime)
{
	// Calculate crosshair velocity factor
	FVector2D WalkSpeedRange(0.0f, 600.0f);
	FVector2D VelocityMultiplierRange(0.0f, 1.0f);
	FVector Velocity = GetVelocity();
	Velocity.Z = 0;

	CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());
	
	// Calculate crosshair in air factor
	if (GetCharacterMovement()->IsFalling())
		// Spread the crosshairs slowly	while in air
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25);
	else
		// Character is on the ground
		CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.0f, DeltaTime, 30.0f);

	// Calculate crosshair aim factor
	if (bAiming)
		// Spread the crosshair quickly while aiming
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.6f, DeltaTime, 30.0f);
	else
		// Character is not aiming 
		CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.0f, DeltaTime, 30.0f); 

	// Calculate crosshair shooting factor. It is true 0.05 seconds after firing
	if (bFiringBullet)
		CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.3f, DeltaTime, 60.0f);
	else
		CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.0f, DeltaTime, 60.0f);

	CrosshairSpreadMultiplier = 0.5f + CrosshairVelocityFactor + CrosshairInAirFactor - CrosshairAimFactor + CrosshairShootingFactor;	
}

void AShooterCharacter::StartCrosshairBulletFire()
{
	bFiringBullet = true;
	GetWorldTimerManager().SetTimer(CrosshairShootTimer, this, &AShooterCharacter::FinishCrosshairBulletFire, ShootTimeDuration);
} 

void AShooterCharacter::FinishCrosshairBulletFire()
{
	bFiringBullet = false; 
}

void AShooterCharacter::FireButtonPressed()
{
	bFireButtonPressed = true;
	StartFireTimer();
}

void AShooterCharacter::FireButtonReleased()
{
	bFireButtonPressed = false;
}

void AShooterCharacter::StartFireTimer()
{
	if (bShouldFire)
	{
		FireWeapon();
		bShouldFire = false;
		GetWorldTimerManager().SetTimer(FireWeaponTimer, this, &AShooterCharacter::ResetFireTimer, FiringRate);
	}
}

void AShooterCharacter::ResetFireTimer()
{
	bShouldFire = true;
	if (bFireButtonPressed)
		StartFireTimer();
}

FVector AShooterCharacter::GetCameraInterpLocation()
{
	const FVector CameraWorldLocation = FollowCamera->GetComponentLocation();
	const FVector CameraForward = FollowCamera->GetForwardVector();

	// Desired Location = CameraWorldLocaation + Forward * A + Up * B
	return (CameraWorldLocation + (CameraForward * CameraInterpDistance) + (FVector(0.0f, 0.0f, CameraInterpElevation))); 
}

void AShooterCharacter::UpdateTheOverlappedItemCount(int8 amount)
{
	if (OverlappedItemCount + amount <= 0)
	{
		OverlappedItemCount = 0;
		bShouldTraceForItem = false;
	}
	else
	{
		OverlappedItemCount += amount;
		bShouldTraceForItem = true;
	}
}


void AShooterCharacter::GetPickupItem(AItem* Item)
{
	auto Weapon = Cast<AWeapon>(Item);
	if (Weapon)
		SwapWeapon(Weapon);
}

// Called when the game starts or when spawned
void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (FollowCamera)
	{
		CameraDefaultFov = GetFollowCamera()->FieldOfView;
		CameraCurrentFov = CameraDefaultFov;
	}

	// Spawn the default weapon and equip it
	EquipWeapon(SpawnDefaultWeapon());
}

// Called every frame
void AShooterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle interpolation for zoom when aiming
	CameraInterpZoom(DeltaTime);

	// Update the turn and lookup rates when aiming
	SetBaseTurnAndLookupRate();

	// Calculate crosshair spread multiplier every frame
	CalculateCrosshairSpread(DeltaTime);

	// Trace for AItems when close enough to the object
	TraceForItems();
}

// Called to bind functionality to input
void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	// Movement Input
	PlayerInputComponent->BindAxis("MoveForward", this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShooterCharacter::MoveRight);

	// Turning At a Rate
	PlayerInputComponent->BindAxis("TurnRate", this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShooterCharacter::LookUpAtRate);

	// Turning With Mouse
	PlayerInputComponent->BindAxis("Turn", this, &AShooterCharacter::TurnWithMouse);
	PlayerInputComponent->BindAxis("LookUp", this, &AShooterCharacter::LookUpWithMouse);

	// Jump Input
	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Released, this, &ACharacter::StopJumping);

	// Fire Input
	PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Pressed, this, &AShooterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("FireButton", EInputEvent::IE_Released, this, &AShooterCharacter::FireButtonReleased);

	// Aiming
	PlayerInputComponent->BindAction("AimingButton", EInputEvent::IE_Pressed, this, &AShooterCharacter::AimingButtonPressed);
	PlayerInputComponent->BindAction("AimingButton", EInputEvent::IE_Released, this, &AShooterCharacter::AimingButtonReleased);
	
	// Selecting
	PlayerInputComponent->BindAction("Select", EInputEvent::IE_Pressed, this, &AShooterCharacter::SelectButtonPressed);
	PlayerInputComponent->BindAction("Select", EInputEvent::IE_Released, this, &AShooterCharacter::SelectButtonReleased);
}
