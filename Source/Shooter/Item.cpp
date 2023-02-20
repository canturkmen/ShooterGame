// Fill out your copyright notice in the Description page of Project Settings.

#include "Item.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/CameraComponent.h"
#include "ShooterCharacter.h"

// Sets default values
AItem::AItem()
	: ItemName(FString("Default")), ItemCount(0), ItemRarity(EItemRarity::EIR_Common), ItemState(EItemState::EIS_Pickup),
	// Item interpolation variables
	ZCurveTime(0.7f), ItemInterpStartLocation(FVector(0.0f)), CameraTargetLocation(FVector(0.0f)), bInterping(false),
	InterpInitialYawOffset(0.0f)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(ItemMesh);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(GetRootComponent());

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AItem::BeginPlay()	
{
	Super::BeginPlay();

	// Hide Pickup widget
	if (PickupWidget)
		PickupWidget->SetVisibility(false);
	
	// Sets ItemStars array based on ItemRarity
	SetActiveStars();

	// Setup Overlap for Area Sphere
	AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AItem::OnSphereOverlap);
	AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AItem::OnSphereEndOverlap); 

	// Set item properties based on state
	SetItemProperties(ItemState);
}

// Called every frame
void AItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle item interping when in the EquipInterping state
	ItemInterp(DeltaTime);
}

void AItem::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
		if (ShooterCharacter)
			ShooterCharacter->UpdateTheOverlappedItemCount(1);
	}
}

void AItem::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor)
	{
		AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
		if (ShooterCharacter)
			ShooterCharacter->UpdateTheOverlappedItemCount(-1);
	}
}

void AItem::SetActiveStars()
{
	for (uint32_t i = 0; i < 5; i++)
		ItemStars.Add(false);

	switch (ItemRarity)
	{
	case EItemRarity::EIR_Damaged:
		ItemStars[0] = true;
		break;
	case EItemRarity::EIR_Common:
		ItemStars[0] = true;
		ItemStars[1] = true;
		break;
	case EItemRarity::EIR_Uncommon:
		ItemStars[0] = true;
		ItemStars[1] = true;
		ItemStars[2] = true;
		break;
	case EItemRarity::EIR_Rare:
		ItemStars[0] = true;
		ItemStars[1] = true;
		ItemStars[2] = true;
		ItemStars[3] = true;
		break;
	case EItemRarity::EIR_Legendary:
		ItemStars[0] = true;
		ItemStars[1] = true;
		ItemStars[2] = true;
		ItemStars[3] = true;
		ItemStars[4] = true;
		break;
	}
}

void AItem::SetItemProperties(EItemState State)
{
	switch (State)
	{
	case EItemState::EIS_Pickup:
		// Set mesh properties
		ItemMesh->SetSimulatePhysics(false);
		ItemMesh->SetEnableGravity(false);
		ItemMesh->SetVisibility(true);
		ItemMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);	

		// Set area sphere properties
		AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); 

		// Set collision box properties
		CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		break;
	case EItemState::EIS_Equipped:
		PickupWidget->SetVisibility(false);
		// Set mesh properties
		ItemMesh->SetSimulatePhysics(false);
		ItemMesh->SetEnableGravity(false);
		ItemMesh->SetVisibility(true);
		ItemMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Set area sphere properties
		AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Set collision box properties
		CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		break;
	case EItemState::EIS_PickedUp:
		break;
	case EItemState::EIS_EquipInterping:
		PickupWidget->SetVisibility(false);
		// Set mesh properties 
		ItemMesh->SetSimulatePhysics(false);
		ItemMesh->SetEnableGravity(false);
		ItemMesh->SetVisibility(true);
		ItemMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Set area sphere properties
		AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Set collision box properties
		CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		break;
	case EItemState::EIS_Falling:
		// Set mesh properties
		ItemMesh->SetSimulatePhysics(true);
		ItemMesh->SetEnableGravity(true);
		ItemMesh->SetVisibility(true);
		ItemMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		ItemMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// Set area sphere properties
		AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Set collision box properties
		CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		break;
	}
}

void AItem::EndItemInterping()
{
	bInterping = false;
	if (Character)
		Character->GetPickupItem(this);

	// Set scale back to normal
	SetActorScale3D(FVector(1.0f));
}

void AItem::ItemInterp(float DeltaTime)
{
	if (!bInterping)
		return;

	if (Character && ItemZCurve)
	{
		// Elapsed time since we started ItemInterpTimer
		const float ElapsedTime = GetWorldTimerManager().GetTimerElapsed(ItemInterpTimer);
		// Get curve value corresponding to elapsed time	 
		const float CurveValue = ItemZCurve->GetFloatValue(ElapsedTime);

		// Get the initial location when the curve started
		FVector ItemLocation = ItemInterpStartLocation;
		// Get location in front of the camera 
		const FVector CameraInterpLocation = Character->GetCameraInterpLocation();

		// Vector from item to interpolation location, X and Y are zeroed out
		const FVector ItemToCamera(0.0f, 0.0f, (CameraInterpLocation - ItemLocation).Z);
		// Scale factor to multiply with CurveValue
		const float DeltaZ = ItemToCamera.Size();

		const FVector CurrentLocation = GetActorLocation();
		// Interpolated X and Y value
		const float InterpXValue = FMath::FInterpTo(CurrentLocation.X, CameraInterpLocation.X, DeltaTime, 30.0f);
		const float InterpYValue = FMath::FInterpTo(CurrentLocation.Y, CameraInterpLocation.Y, DeltaTime, 30.0f);

		// Set X and Y of ItemLocation to interpolated location
		ItemLocation.X = InterpXValue;
		ItemLocation.Y = InterpYValue;

		// Update the Z location of the item with respect to the CurveValue and DeltaZ
		ItemLocation.Z += CurveValue * DeltaZ;

		SetActorLocation(ItemLocation, true, nullptr, ETeleportType::TeleportPhysics);

		// Camera rotation this frame
		const FRotator CameraRotation(Character->GetFollowCamera()->GetComponentRotation());

		// Item Rotation's yaw is set to the camera's new yaw value + offset
		FRotator ItemRotation(0.0f, CameraRotation.Yaw + InterpInitialYawOffset, 0.0f);
		SetActorRotation(ItemRotation, ETeleportType::TeleportPhysics);

		if (ItemScaleCurve)
		{
			const float ScaleCurveValue = ItemScaleCurve->GetFloatValue(ElapsedTime);
			SetActorScale3D(FVector(ScaleCurveValue));
		}
	}
}

void AItem::SetItemState(EItemState itemState)
{
	ItemState = itemState;
	SetItemProperties(itemState);
}

void AItem::StartItemInterping(AShooterCharacter* ShooterChar)
{
	Character = ShooterChar;
	ItemInterpStartLocation = GetActorLocation();
	bInterping = true;
	SetItemState(EItemState::EIS_EquipInterping);

	GetWorldTimerManager().SetTimer(ItemInterpTimer, this, &AItem::EndItemInterping, ZCurveTime);

	// Get initial yaw of the camera and the item
	const float CameraRotationYaw = Character->GetFollowCamera()->GetComponentRotation().Yaw;
	const float ItemRotationYaw = GetActorRotation().Yaw;

	// Calculate the initial yaw offset between camera and item
	InterpInitialYawOffset = ItemRotationYaw - CameraRotationYaw;
}

