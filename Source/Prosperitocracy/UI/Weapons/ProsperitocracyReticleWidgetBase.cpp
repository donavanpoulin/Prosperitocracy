// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/Weapons/ProsperitocracyReticleWidgetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ProsperitocracyCharacter.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ProsperitocracyLogChannels.h"
#include "UI/Weapons/ProsperitocracyHitMarkerWidget.h"
#include "Weapons/ProsperitocracyWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyReticleWidgetBase)

UProsperitocracyReticleWidgetBase::UProsperitocracyReticleWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UProsperitocracyReticleWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// The visual side is the widget blueprint's: it lays out exactly three layers and this class moves
	// two of them. Found by name, so the blueprint owns the look and this owns the behaviour.
	if (WidgetTree)
	{
		DotImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Dot")));
		CircleImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("Circle")));
		HitMarkerWidget = Cast<UProsperitocracyHitMarkerWidget>(WidgetTree->FindWidget(TEXT("HitMarker")));

		if (!HitMarkerWidget)
		{
			// Loud, because it is an authoring gap rather than a state: the damage still lands, the
			// sounds still play, and no X is ever drawn.
			UE_LOG(LogProsperitocracy, Warning, TEXT("[Reticle] no widget named 'HitMarker' in %s — hits will make no X (the sounds are unaffected)."), *GetNameSafe(GetClass()));
		}
	}
}

void UProsperitocracyReticleWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Listen to the man's own damage, once he exists (one look per frame until then).
	BindToHitMarkersOnce();

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

	// At the hip there is no ring, and a marker is PART of the ring's UI — so nothing is drawn, and no
	// marker is kept waiting for the ring to come back (his call, 2026-09-25). The SOUND already played
	// when the hit arrived: at the hip, hearing it is what tells you that you connected.
	if (AimingAlpha <= 0.01f)
	{
		bMarkerLive = false;
		bMarkerDrawn = false;
		HideHitMarker();
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

	const FVector2D CircleOffset = ComputeCircleScreenPosition() - ScreenCenter;
	CircleImage->SetRenderTranslation(CircleOffset);

	// The X is measured in the RING's own size, read off the ring image itself — never a second number
	// kept somewhere else, so "inside the ring" stays true even if the ring's own art is ever resized.
	const FVector2D RingLocalSize = CircleImage->GetCachedGeometry().GetLocalSize();
	const float RingRadius = FMath::Max(RingLocalSize.X, RingLocalSize.Y) * 0.5f;

	UpdateHitMarker(InDeltaTime, AimingAlpha, CircleOffset, RingRadius);
}

void UProsperitocracyReticleWidgetBase::BindToHitMarkersOnce()
{
	if (bBoundToHitMarkers)
	{
		return;
	}

	APawn* Pawn = GetOwningPlayerPawn();
	UProsperitocracyAbilitySystemComponent* AbilitySystemComponent = Pawn
		? Cast<UProsperitocracyAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
		: nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	// The man's OWN component, so anything broadcast on it is his damage and only his damage.
	AbilitySystemComponent->OnHitMarker.AddDynamic(this, &ThisClass::HandleHitMarker);
	bBoundToHitMarkers = true;
}

void UProsperitocracyReticleWidgetBase::HandleHitMarker(EProsperitocracyHitMarkerKind Kind)
{
	// ONE marker at a time (his spec): a fresh hit RESTARTS the X's fade rather than stacking another X
	// on top of one that is still fading — whichever kind was fading, a kill's included.
	//
	// (A kill's X is not protected while it lasts. It tried to be, and his own report — 2026-09-25,
	// "some x's just don't go through" while shooting slowly — is why it is not: hits landing during a
	// kill's fade were being swallowed. A hit is always an answer, and the newest answer is the one on
	// screen. The blow that kills still shows ONE X, the big one, because its kill arrives in the same
	// frame as its own hit and takes the marker from it there.)
	PlayHitMarkerSound(Kind);

	LiveMarkerKind = Kind;
	MarkerAgeSeconds = 0.0f;
	bMarkerDrawn = false;
	bMarkerLive = true;
}

void UProsperitocracyReticleWidgetBase::UpdateHitMarker(float DeltaTime, float AimingAlpha, const FVector2D& CircleOffset, float RingRadius)
{
	if (!HitMarkerWidget)
	{
		return;
	}

	if (!bMarkerLive || RingRadius <= 0.0f)
	{
		// Nothing to show — and HideHitMarker tells the widget only on the frame a marker actually ends,
		// so a widget that is already hidden is not re-collapsed every frame for nothing.
		HideHitMarker();
		return;
	}

	// THE MARKER'S LIFE STARTS ON THE FIRST FRAME IT IS ACTUALLY ON SCREEN — never on the frame it
	// arrived.
	//
	// It used to run from arrival, and THAT is what made whole markers vanish. One long frame — and the
	// first shot after a lull is exactly when one lands, with a fresh shader, flash, impact and sound
	// being built — pushed the age past the marker's entire lifetime on its very first evaluation, so it
	// was expired before it was ever drawn: born and dead inside one frame, nothing on the screen. His
	// report, 2026-09-25: "the first shot... it's just not there, next shot it is though". A marker now
	// gets its full-strength frame whatever the frame time was, and the clock starts after it.
	if (bMarkerDrawn)
	{
		MarkerAgeSeconds += DeltaTime;
	}
	bMarkerDrawn = true;

	const float Lifetime = (LiveMarkerKind == EProsperitocracyHitMarkerKind::Kill) ? KillMarkerLifetimeSeconds : HitMarkerLifetimeSeconds;
	if (MarkerAgeSeconds >= Lifetime)
	{
		// Spent: it goes, and the next hit starts its own from full strength.
		bMarkerLive = false;
		bMarkerDrawn = false;
		HideHitMarker();
		return;
	}

	// Full strength for the first stretch, then the fade — a marker that starts fading the instant it
	// appears reads as a flicker rather than as a hit.
	const float FadeStart = Lifetime * HitMarkerHoldFraction;
	const float Fade = (MarkerAgeSeconds <= FadeStart)
		? 1.0f
		: 1.0f - ((MarkerAgeSeconds - FadeStart) / (Lifetime - FadeStart));

	// Part of the ring's UI, in both senses: it fades in with the ring's own ADS blend (the very number
	// the ring's own opacity reads) and it is moved by the very value the ring is moved by — so the X is
	// inside the ring by construction, never by a second calculation that could disagree with it.
	const float RingVisibility = FMath::Clamp(AimingAlpha * 2.0f, 0.0f, 1.0f);

	HitMarkerWidget->SetMarker(LiveMarkerKind, Fade * RingVisibility, RingRadius);
	HitMarkerWidget->SetRenderTranslation(CircleOffset);
}

void UProsperitocracyReticleWidgetBase::HideHitMarker()
{
	// Already hidden: nothing to tell the widget, and it is asked its own state rather than kept in a
	// flag of its own — one owner for "is the X on the ring".
	if (!HitMarkerWidget || HitMarkerWidget->MarkerOpacity <= 0.0f)
	{
		return;
	}

	HitMarkerWidget->SetMarker(HitMarkerWidget->Kind, 0.0f, HitMarkerWidget->RingRadius);
}

USoundBase* UProsperitocracyReticleWidgetBase::GetHitMarkerSound(EProsperitocracyHitMarkerKind Kind) const
{
	switch (Kind)
	{
	case EProsperitocracyHitMarkerKind::Kill:
		return KillHitSound;

	case EProsperitocracyHitMarkerKind::Half:
		return HalfHitSound;

	case EProsperitocracyHitMarkerKind::Full:
	default:
		return FullHitSound;
	}
}

void UProsperitocracyReticleWidgetBase::PlayHitMarkerSound(EProsperitocracyHitMarkerKind Kind)
{
	USoundBase* Sound = GetHitMarkerSound(Kind);
	if (!Sound)
	{
		// The X still shows; it is simply silent until the sound is put on the widget. Not a failure —
		// it is exactly what the first build after this one looks like.
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// The cap (his call, 2026-09-25): the same marker's sound will not restart faster than this, so a
	// burn ticking on a whole horde reads as one sound being held rather than a machine-gun.
	const int32 KindIndex = static_cast<int32>(Kind);
	if (Now - LastHitMarkerSoundTimes[KindIndex] < HitMarkerSoundRetriggerSeconds)
	{
		return;
	}
	LastHitMarkerSoundTimes[KindIndex] = Now;

	// A blow that kills is ONE blow: its sound takes over from the hit sound that started an instant
	// before it (the pipeline tells the hit, the death tells the kill, in the same frame) — which is
	// also why a kill draws one X and not a hit's X with a bigger one over it.
	if (Kind == EProsperitocracyHitMarkerKind::Kill && PlayingHitMarkerSound.IsValid())
	{
		PlayingHitMarkerSound->Stop();
	}

	PlayingHitMarkerSound = UGameplayStatics::SpawnSound2D(this, Sound, HitMarkerSoundVolume);
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

	// The circle sits exactly where the next shot lands: THE MAN'S AIM, which is his own turn plus the
	// gun's own numbers. With a gun in hand the gun answers the direction — literally the vector the
	// bullet flies along — so the circle and the bullet cannot disagree. With nothing in hand it is
	// the plain camera aim.
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

	// BEHIND THE VIEW — there is no landing point on the screen, and the ring must NEVER be dropped
	// onto the dot to say so. A hard turn is exactly this case: the man is still swinging round, so
	// his gun's landing point is momentarily behind where the player is looking. Parking the ring at
	// the centre is a one-frame teleport that reads as the aim snapping, and it is a lie besides —
	// the gun IS still pointing back there. So the ring goes to the EDGE, in the direction it left
	// by, which is what actually happened.
	UE_LOG(LogProsperitocracy, Log, TEXT("[Reticle] the landing point is behind the view — the ring is pinned to the edge, never the dot"));

	int32 ViewportSizeX(0);
	int32 ViewportSizeY(0);
	PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

	const FVector2D ScreenCentre(ViewportSizeX * 0.5, ViewportSizeY * 0.5);

	// Which way it went, in the VIEW's own frame — right and up as the player sees them. Read off the
	// camera's rotation rather than the projection, because the projection is what just failed.
	const FVector ViewDirection = CameraRotation.UnrotateVector(AimDirection);
	const FVector2D ScreenDirection(ViewDirection.X, -ViewDirection.Y); // screen Y grows downward
	if (ScreenDirection.IsNearlyZero())
	{
		// Straight behind the camera: there is no side to go to, and inventing one would be a second
		// lie. The bottom edge is the one place that can never be mistaken for the dot.
		return FVector2D(ScreenCentre.X, ViewportSizeY);
	}

	const FVector2D Direction = ScreenDirection.GetSafeNormal();
	const float HalfWidth = FMath::Max(1.0, ScreenCentre.X);
	const float HalfHeight = FMath::Max(1.0, ScreenCentre.Y);
	const float EdgeScale = FMath::Min(
		FMath::Abs(Direction.X) > KINDA_SMALL_NUMBER ? HalfWidth / FMath::Abs(Direction.X) : BIG_NUMBER,
		FMath::Abs(Direction.Y) > KINDA_SMALL_NUMBER ? HalfHeight / FMath::Abs(Direction.Y) : BIG_NUMBER);

	return ScreenCentre + Direction * EdgeScale;
}

float UProsperitocracyReticleWidgetBase::GetCircleScreenRadius() const
{
	// One fixed radius for every weapon. The circle never grows or shrinks: a gun's inaccuracy is the
	// circle's position, never its size. [TUNE]
	return 45.0f;
}
