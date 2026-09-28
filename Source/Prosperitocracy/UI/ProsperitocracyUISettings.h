// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"

#include "ProsperitocracyUISettings.generated.h"

/**
 * FProsperitocracyUIColour
 *
 * ONE colour the UI paints with, as ONE number: 0xRRGGBB — the same shape every colour in this game
 * carries (an armour's trim is one hex per region, a class's own colour is one hex). A colour written as
 * three values would be three things to carry where one does.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyUIColour
{
	GENERATED_BODY()

	/**
	 * What the colour is asked for BY: the game's own name for the thing it belongs to, never a second
	 * vocabulary minted for the UI (the two damage types are the tags damage already carries).
	 */
	UPROPERTY(EditAnywhere, meta = (Categories = "Damage.Type,Status"))
	FGameplayTag Name;

	/** The colour itself, as one hex number: 0xRRGGBB. 0 is black, which is a real colour to pick. */
	UPROPERTY(EditAnywhere)
	int32 Hex = 0xFFFFFF;
};

/**
 * UProsperitocracyUISettings
 *
 * THE ONE SPOT the game's UI colours are set at (his design, 2026-09-27): a colour that belongs to no
 * THING lives here once, and everything that paints with one reads it from here — either STRAIGHT (the
 * damage numbers use the Impact colour exactly as it is set here), or as a BASE it modifies (the same
 * number fades: the same colour, less alpha).
 *
 * A colour that belongs to a THING keeps living on the thing, exactly as it already does: an armour's
 * trim is a row on the armour, a class's colour is on the class. Presence is scope, and it is the same
 * rule here — a colour that belongs to no thing has no home of its own, so this is its home. That is
 * why the class and currency colours are NOT rows here: they have homes, and a second copy of a colour
 * is the one thing this palette exists to prevent.
 *
 * A settings class rather than an asset, on purpose: one instance, always loaded, nothing to reference
 * from anywhere and nothing to lose — Project Settings → Prosperitocracy UI is the one spot, and
 * `Config/DefaultGame.ini` is the record of what is set there.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Prosperitocracy UI"))
class UProsperitocracyUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UProsperitocracyUISettings();

	/** The one spot itself: the single object every reader asks, wherever it lives. */
	static const UProsperitocracyUISettings& Get();

	/**
	 * The colour of the named thing, AS IT IS SET HERE — used straight unless the caller takes it as a
	 * base and modifies it. A name that has no colour yet answers with white and says so in the log,
	 * rather than letting a wrong colour pass for a decided one.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|UI")
	FLinearColor GetUIColour(FGameplayTag Name) const;

	/**
	 * Every colour the UI paints with. APPEND a row to add a colour — never reorder or rewrite the ones
	 * already set, so a colour keeps meaning what it means.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Colours")
	TArray<FProsperitocracyUIColour> Colours;
};
