// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/ProsperitocracyHUD.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_ContestedHealth.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "AbilitySystem/ProsperitocracyAbilitySystemComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"
#include "UI/ProsperitocracyDamageNumbersLayer.h"
#include "Weapons/ProsperitocracyBloodBlade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProsperitocracyHUD)

AProsperitocracyHUD::AProsperitocracyHUD()
{
	// It draws every frame through DrawHUD; it has no tick of its own.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

const UProsperitocracyHealthSet* AProsperitocracyHUD::GetOwningHealthSet() const
{
	APawn* Pawn = GetOwningPawn();
	const UAbilitySystemComponent* AbilitySystemComponent = Pawn ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn) : nullptr;
	return AbilitySystemComponent ? AbilitySystemComponent->GetSet<UProsperitocracyHealthSet>() : nullptr;
}

AProsperitocracyBloodBlade* AProsperitocracyHUD::FindOwningBloodBlade() const
{
	APawn* Pawn = GetOwningPawn();
	if (!Pawn)
	{
		return nullptr;
	}

	// Found by asking each of the body's channels what it is holding: the same walk the blade's own
	// refuel and the equip path use, so no list of component names lives anywhere.
	TArray<UChildActorComponent*> Channels;
	Pawn->GetComponents(Channels);

	for (UChildActorComponent* Channel : Channels)
	{
		if (AProsperitocracyBloodBlade* Blade = Channel ? Cast<AProsperitocracyBloodBlade>(Channel->GetChildActor()) : nullptr)
		{
			return Blade;
		}
	}

	return nullptr;
}

void AProsperitocracyHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// The readout is bound here, once a frame, because binding belongs to the man's OWN component and this
	// HUD is the thing that has one. The numbers themselves are drawn by the Slate layer (BeginPlay put it
	// on screen), not by anything below this line.
	EnsureBoundToDamageFeedback();

	APawn* Pawn = GetOwningPawn();
	const UAbilitySystemComponent* AbilitySystemComponent = Pawn ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn) : nullptr;
	const UProsperitocracyHealthSet* HealthSet = GetOwningHealthSet();
	if (!AbilitySystemComponent || !HealthSet)
	{
		// No body, or a body with no health of its own: nothing to show, and nothing is drawn rather than
		// an empty bar pretending to be a bar.
		return;
	}

	// The health, read through the ONE evaluator like every other number in the game (Health is a row).
	const float Health = UProsperitocracyStatSystemStatics::GetStatFinal(AbilitySystemComponent, EProsperitocracyStat::Health);

	// Max health is the OTHER HALF of that same row: it is GAS's ceiling attribute, not a row of its own,
	// and AProsperitocracyStatSystemStatics::ApplyBlockBodyRows writes the Health row into both so a row
	// of 300 is a body of 300/300.
	const float MaxHealth = HealthSet->GetMaxHealth();
	if (MaxHealth <= 0.0f)
	{
		return;
	}

	// And the contested part: the health this body lost and can still win back by landing damage.
	const float Contested = UProsperitocracyGameplayAbility_ContestedHealth::GetContestedHealth(Pawn);

	// The bar: the health you have, the contested part of it that is still winnable, and then nothing at
	// all. The empty stretch is deliberately left alone — the bar shows what you HOLD.
	const float HealthFraction = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
	const float ContestedFraction = FMath::Clamp(Contested / MaxHealth, 0.0f, 1.0f - HealthFraction);

	const float ScreenWidth = Canvas->SizeX;
	const float ScreenHeight = Canvas->SizeY;

	const float BarWidth = ScreenWidth * BarWidthShareOfScreen;
	const float BarLeft = BarLeftMarginPixels;
	const float BarTop = ScreenHeight - BarBottomMarginPixels - BarHeightPixels;
	const float Border = BorderThicknessPixels;

	const float HealthWidth = BarWidth * HealthFraction;
	const float ContestedWidth = BarWidth * ContestedFraction;

	// The border first, as four thin edges rather than a filled rectangle: a filled one would paint over
	// the stretch that is supposed to stay empty.
	DrawRect(FLinearColor::Black, BarLeft - Border, BarTop - Border, BarWidth + (Border * 2.0f), Border);
	DrawRect(FLinearColor::Black, BarLeft - Border, BarTop + BarHeightPixels, BarWidth + (Border * 2.0f), Border);
	DrawRect(FLinearColor::Black, BarLeft - Border, BarTop, Border, BarHeightPixels);
	DrawRect(FLinearColor::Black, BarLeft + BarWidth, BarTop, Border, BarHeightPixels);

	// Then the two things you are holding: your health in its dark red, and the contested part of it in
	// white, sat at the end of the red so the whole bar length is what the body is holding right now.
	const FLinearColor HealthColor = FLinearColor::FromSRGBColor(FColor(
		(HealthColorHex >> 16) & 0xFF, (HealthColorHex >> 8) & 0xFF, HealthColorHex & 0xFF));

	DrawRect(HealthColor, BarLeft, BarTop, HealthWidth, BarHeightPixels);
	DrawRect(FLinearColor::White, BarLeft + HealthWidth, BarTop, ContestedWidth, BarHeightPixels);

	//~ The BLOOD BAR -------------------------------------------------------------------------------
	//
	// The sword's own pool, and ONLY a body carrying a blade has one: no blade, no bar — which is how
	// the Reclaimer alone gets it without anything naming the class. The same size as the health bar
	// turned on its side, in the bottom RIGHT corner, and it reads the same way: what the body is
	// HOLDING is drawn and the rest is left alone.
	AProsperitocracyBloodBlade* Blade = FindOwningBloodBlade();
	if (!Blade)
	{
		return;
	}

	const float MaxBlood = Blade->GetMaxBlood();
	if (MaxBlood <= 0.0f)
	{
		return;
	}

	// The REAL number, and then the number that is DRAWN. The pool moves in whichever tiny steps the
	// bleed and the hits give it, so the bar chases it and falls and rises smoothly instead of stepping
	// — that is a fact about the picture, which is why it is kept on the picture.
	const float BloodFraction = FMath::Clamp(Blade->GetBlood() / MaxBlood, 0.0f, 1.0f);
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	SmoothedBloodFraction = FMath::FInterpTo(SmoothedBloodFraction, BloodFraction, DeltaSeconds, BloodBarSmoothingRate);

	const bool bBloodMode = Blade->IsBloodModeOn();

	const float BloodThickness = BloodBarThicknessPixels;
	const float BloodLength = ScreenWidth * BloodBarLengthShareOfScreen;
	const float BloodBarRight = ScreenWidth - BloodBarRightMarginPixels;
	const float BloodBarLeft = BloodBarRight - BloodThickness;
	const float BloodBarBottom = ScreenHeight - BloodBarBottomMarginPixels;
	const float BloodBarTop = BloodBarBottom - BloodLength;

	// The mode paints the panel behind it first — the dark red that sits BETWEEN the blood and the
	// border — so the strip reads as lit the moment it is on. Off, nothing is painted and the empty
	// stretch stays transparent, exactly as the health bar's does.
	if (bBloodMode)
	{
		DrawRect(FLinearColor::FromSRGBColor(FColor(
			(BloodModeBackdropHex >> 16) & 0xFF, (BloodModeBackdropHex >> 8) & 0xFF, BloodModeBackdropHex & 0xFF)),
			BloodBarLeft, BloodBarTop, BloodThickness, BloodLength);
	}

	// The blood itself, filling from the BOTTOM up: it is a pool the body is holding, so the bottom is
	// where it sits.
	const float BloodHeight = BloodLength * SmoothedBloodFraction;
	const FLinearColor BloodColor = FLinearColor::FromSRGBColor(FColor(
		(BloodColorHex >> 16) & 0xFF, (BloodColorHex >> 8) & 0xFF, BloodColorHex & 0xFF));
	DrawRect(BloodColor, BloodBarLeft, BloodBarBottom - BloodHeight, BloodThickness, BloodHeight);

	// And the border: black normally, and the DARKEST of the three reds while the mode is on — blood
	// lightest, its backdrop between, its border darkest.
	const FLinearColor BloodBorderColor = bBloodMode
		? FLinearColor::FromSRGBColor(FColor(
			(BloodModeBorderHex >> 16) & 0xFF, (BloodModeBorderHex >> 8) & 0xFF, BloodModeBorderHex & 0xFF))
		: FLinearColor::Black;

	DrawRect(BloodBorderColor, BloodBarLeft - Border, BloodBarTop - Border, BloodThickness + (Border * 2.0f), Border);
	DrawRect(BloodBorderColor, BloodBarLeft - Border, BloodBarBottom, BloodThickness + (Border * 2.0f), Border);
	DrawRect(BloodBorderColor, BloodBarLeft - Border, BloodBarTop, Border, BloodLength);
	DrawRect(BloodBorderColor, BloodBarLeft + BloodThickness, BloodBarTop, Border, BloodLength);
}

void AProsperitocracyHUD::EnsureBoundToDamageFeedback()
{
	// The body the man is playing RIGHT NOW, re-read every frame: a respawn or a fresh pawn is a
	// different component, and the readout follows it rather than staying pointed at one that is gone.
	APawn* Pawn = GetOwningPawn();
	UProsperitocracyAbilitySystemComponent* AbilitySystemComponent = Pawn
		? Cast<UProsperitocracyAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
		: nullptr;

	if (AbilitySystemComponent == BoundAbilitySystemComponent)
	{
		return;
	}

	if (BoundAbilitySystemComponent)
	{
		BoundAbilitySystemComponent->OnDamageFeedback.RemoveDynamic(this, &ThisClass::HandleDamageFeedback);
	}

	BoundAbilitySystemComponent = AbilitySystemComponent;

	if (BoundAbilitySystemComponent)
	{
		BoundAbilitySystemComponent->OnDamageFeedback.AddDynamic(this, &ThisClass::HandleDamageFeedback);
	}
}

void AProsperitocracyHUD::BeginPlay()
{
	Super::BeginPlay();

	AddDamageNumbersLayer();
}

void AProsperitocracyHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveDamageNumbersLayer();

	Super::EndPlay(EndPlayReason);
}

void AProsperitocracyHUD::AddDamageNumbersLayer()
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport || DamageNumbersLayer.IsValid())
	{
		return;
	}

	// The numbers are drawn by SLATE and not on this HUD's canvas (see the layer's own header for why: a
	// canvas outline is four extra full-glyph passes under the fill, and those passes darkened the fade).
	// The layer is added over everything the canvas draws, so a number is never behind the bars.
	SAssignNew(DamageNumbersLayer, SProsperitocracyDamageNumbersLayer)
		.OwningPlayerController(GetOwningPlayerController());

	Viewport->AddViewportWidgetContent(DamageNumbersLayer.ToSharedRef(), /*ZOrder=*/ 10);
}

void AProsperitocracyHUD::RemoveDamageNumbersLayer()
{
	if (!DamageNumbersLayer.IsValid())
	{
		return;
	}

	if (UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(DamageNumbersLayer.ToSharedRef());
	}

	DamageNumbersLayer.Reset();
}

void AProsperitocracyHUD::HandleDamageFeedback(const FProsperitocracyDamageFeedback& Feedback)
{
	// Straight to the layer that draws them. A payload with no numbers is a real, ordinary thing here (the
	// killing blow's own answer rides the same door and owes none) and the layer ignores it — so the rule
	// lives in ONE place, where the numbers are held, and not twice.
	if (DamageNumbersLayer.IsValid())
	{
		DamageNumbersLayer->AddNumber(Feedback);
	}
}

// THE DAMAGE NUMBERS USED TO BE DRAWN HERE, on this HUD's canvas. They moved out to their own Slate layer
// (SProsperitocracyDamageNumbersLayer) on 2026-09-27: a canvas outline is four extra full-glyph passes
// under the fill, and those four passes took the world away behind every digit, so a number went DARK as
// it faded instead of turning translucent. Everything that was here — the pooling, the world-to-screen
// projection, the rise and the fade, the one-space gap, the size band — lives there now, in one place.
