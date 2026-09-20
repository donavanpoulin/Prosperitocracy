// Copyright Prosperitocracy. All Rights Reserved.

#include "UI/ProsperitocracyHUD.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility_ContestedHealth.h"
#include "AbilitySystem/Attributes/ProsperitocracyHealthSet.h"
#include "Engine/Canvas.h"
#include "GameFramework/Pawn.h"
#include "Stats/ProsperitocracyStat.h"
#include "Stats/ProsperitocracyStatSystemStatics.h"

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

void AProsperitocracyHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

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
}
