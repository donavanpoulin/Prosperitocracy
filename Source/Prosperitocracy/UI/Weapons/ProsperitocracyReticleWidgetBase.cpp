// Copyright Prosperitocracy. All Rights Reserved.

#include "ProsperitocracyReticleWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/Image.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyReticleWidgetBase)

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

	// An ADS-only aid (Design/ui.md): hipfire shows the fixed dot alone, and the circle fades in with
	// the template's own ADS blend — the same number the posture multiplier reads, so the aid and the
	// aim can never disagree. Twice the alpha makes it fully opaque at half a transition. [TUNE]
	const AProsperitocracyCharacter* Character = GetOwningCharacter();
	const float AimingAlpha = Character ? Character->GetAimingAlpha() : 0.0f;
	CircleImage->SetRenderOpacity(FMath::Clamp(AimingAlpha * 2.0f, 0.0f, 1.0f));
	if (AimingAlpha <= 0.01f)
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

	// The circle sits exactly where the next shot lands: the camera aim plus the gun's drift. With a
	// gun in hand the gun answers the direction — literally the vector the bullet flies along — so the
	// circle and the bullet cannot disagree. With nothing in hand it is the plain camera aim.
	FVector CameraPosition;
	FRotator CameraRotation;
	PC->PlayerCameraManager->GetCameraViewPoint(CameraPosition, CameraRotation);

	const FVector AimDirection = WeaponInstance ? WeaponInstance->GetShotDirection() : CameraRotation.Vector();

	// A reference distance in open air: far enough that only real cover snaps the circle in.
	constexpr float AimDistance = 10000.0f;
	const FVector AimTarget = CameraPosition + AimDirection * AimDistance;

	// Cover snap: if something genuinely blocks the shot, the circle sits on that surface instead,
	// because that is where the bullet will actually hit. The same channel the guns trace, and the
	// same actors they ignore (the shooter and everything attached to it).
	FVector ProjectionTarget = AimTarget;
	if (UWorld* World = GetWorld())
	{
		AActor* OwningPawn = PC->GetPawn();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ReticleCoverSnap), /*bTraceComplex=*/ true, /*IgnoreActor=*/ OwningPawn);
		if (OwningPawn)
		{
			TArray<AActor*> AttachedActors;
			OwningPawn->GetAttachedActors(AttachedActors);
			Params.AddIgnoredActors(AttachedActors);
		}

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, CameraPosition, AimTarget, ECC_Visibility, Params) && Hit.bBlockingHit)
		{
			ProjectionTarget = Hit.ImpactPoint;
		}
	}

	FVector2D ScreenPosition;
	if (PC->ProjectWorldLocationToScreen(ProjectionTarget, ScreenPosition, true))
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
