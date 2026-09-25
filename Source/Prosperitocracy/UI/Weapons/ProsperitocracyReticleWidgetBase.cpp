// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyReticleWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ProsperitocracyCharacter.h"
#include "Components/Image.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyReticleWidgetBase)

namespace ProsperitocracyReticle
{
	/**
	 * How fast the circle fades in and out with the gun's own aiming state, per second. The state is a
	 * yes or no; the fade is this. [TUNE]
	 */
	constexpr float CircleFadeRate = 6.0f;
}

UProsperitocracyReticleWidgetBase::UProsperitocracyReticleWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProsperitocracyReticleWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// The visual side is the widget blueprint's: it lays out exactly two images and this class moves
	// one of them. Found by name, so the blueprint owns the look and this owns the behaviour.
	if (WidgetTree)
	{
		DotImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Dot")));
		CircleImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Circle")));
	}
}

void UProsperitocracyReticleWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Follow the gun in hand: one universal reticle, no per-weapon wiring, and no stale gun.
	AcquireWeaponFromCharacter();

	if (!CircleImage)
	{
		return;
	}

	// An ADS-only aid (Design/ui.md): hipfire shows the fixed dot alone. What it follows is THE GUN'S
	// OWN aiming state, eased here so the fade is smooth while the state itself is a plain yes or no —
	// the gun owns whether it is being aimed, and this is a read of it, never a second opinion. [TUNE]
	const float TargetOpacity = (WeaponInstance && WeaponInstance->IsAiming()) ? 1.0f : 0.0f;
	CircleOpacity = FMath::FInterpTo(CircleOpacity, TargetOpacity, InDeltaTime, ProsperitocracyReticle::CircleFadeRate);
	CircleImage->SetRenderOpacity(CircleOpacity);
	if (CircleOpacity <= 0.01f)
	{
		return;
	}

	// The widget lays the circle out at screen centre with a 0.5/0.5 pivot, so translating it by
	// (landing point - screen centre) is what puts it on the landing point. The dot never moves.
	APlayerController* PC = GetOwningPlayer();
	int32 ViewportSizeX(0);
	int32 ViewportSizeY(0);
	if (PC)
	{
		PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	}
	const FVector2D ScreenCenter(ViewportSizeX * 0.5, ViewportSizeY * 0.5);

	CircleImage->SetRenderTranslation(ComputeCircleScreenPosition() - ScreenCenter);
}

AProsperitocracyCharacter* UProsperitocracyReticleWidgetBase::GetOwningCharacter() const
{
	return Cast<AProsperitocracyCharacter>(GetOwningPlayerPawn());
}

void UProsperitocracyReticleWidgetBase::AcquireWeaponFromCharacter()
{
	// The gun in hand is the rig's answer, asked through the character — the same door the aim pose
	// reads. Nothing here inspects meshes or guesses from the slot; there is one answer.
	AProsperitocracyWeapon* GunInHand = nullptr;
	if (const AProsperitocracyCharacter* Character = GetOwningCharacter())
	{
		GunInHand = Character->GetGunWeaponInHand();
	}

	if (GunInHand != WeaponInstance)
	{
		InitializeFromWeapon(GunInHand);
	}
}

void UProsperitocracyReticleWidgetBase::InitializeFromWeapon(AProsperitocracyWeapon* InWeapon)
{
	WeaponInstance = InWeapon;
	OnWeaponInitialized();
}

FVector2D UProsperitocracyReticleWidgetBase::ComputeCircleScreenPosition() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->PlayerCameraManager)
	{
		return FVector2D::ZeroVector;
	}

	// THE SHOT'S OWN LINE, asked of the gun: the muzzle's position, the barrel's own axis, and the ONE
	// trace the bullet itself travels down. The circle goes on that line's end — so what the circle sits
	// on is what a shot hits, and a wall between the player and his target puts the circle on the wall
	// because that is genuinely where the bullet stops. There is no second look at the world here.
	FVector LineStart = FVector::ZeroVector;
	FVector LineEnd = FVector::ZeroVector;
	FHitResult Hit;
	FRotator CameraRotation;

	if (WeaponInstance)
	{
		WeaponInstance->GetShotLine(LineStart, LineEnd, Hit);
	}
	else
	{
		// Nothing in hand is nothing to shoot, so the camera's own aim is the only honest answer left.
		FVector CameraPosition;
		PC->PlayerCameraManager->GetCameraViewPoint(CameraPosition, CameraRotation);
		LineStart = CameraPosition;
		LineEnd = CameraPosition + CameraRotation.Vector() * 10000.0f;
	}

	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(LineEnd, ScreenPosition, true))
	{
		return ScreenPosition;
	}

	// Nothing to project (the view is not set up): the circle belongs at centre, on the dot.
	int32 ViewportSizeX(0);
	int32 ViewportSizeY(0);
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
	return FVector2D(ViewportSizeX * 0.5, ViewportSizeY * 0.5);
}

float UProsperitocracyReticleWidgetBase::GetCircleScreenRadius() const
{
	// One fixed radius for every weapon. The circle never grows or shrinks: a gun's inaccuracy is the
	// circle's position, never its size. [TUNE]
	return 45.0f;
}
