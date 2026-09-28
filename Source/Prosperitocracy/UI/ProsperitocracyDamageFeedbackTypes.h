// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/ProsperitocracyHitMarkerTypes.h"

#include "ProsperitocracyDamageFeedbackTypes.generated.h"

/**
 * FProsperitocracyDamageNumber
 *
 * ONE number the player's screen shows: what a single damage LINE took off, and that line's own type.
 *
 * The type is the whole reason this is a pair rather than a lone float — the number is painted in the
 * colour of the damage it shows, and it reads that colour straight off the one spot the UI's colours are
 * set at (UProsperitocracyUISettings). The number never carries a colour of its own, and never modifies
 * the one it reads.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyDamageNumber
{
	GENERATED_BODY()

	/** Exactly one of Damage.Type.Impact / Damage.Type.Piercing — a damage line IS its type. */
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag Type;

	/** What that line actually took off, after the gate, the resist and the falloff. */
	UPROPERTY(BlueprintReadOnly)
	float Amount = 0.0f;
};

/**
 * FProsperitocracyDamageFeedback
 *
 * Everything the man whose damage it was is owed about that damage, in ONE payload: which of the three
 * markers the blow earns, what each line took off, and where the numbers come up.
 *
 * ONE payload and ONE hop rather than two: a hit marker and a damage number are the same answer told
 * twice, and a horde on fire lands several answers a second — so they ride together, or the channel
 * pays for both twice.
 */
USTRUCT(BlueprintType)
struct FProsperitocracyDamageFeedback
{
	GENERATED_BODY()

	/**
	 * Where the blow comes up, in the world: the point the damage LANDED when it struck a part, and the
	 * body's own origin when it reached the body itself (a burn, a command that deals at no particular
	 * place). Read only when there are numbers to show.
	 */
	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	/**
	 * What each line took off — one entry per line, so a blow of both types carries TWO numbers and each
	 * is painted in its own type's colour.
	 */
	UPROPERTY(BlueprintReadOnly)
	TArray<FProsperitocracyDamageNumber> Numbers;

	/** Which of the three markers the blow earns (see EProsperitocracyHitMarkerKind). */
	UPROPERTY(BlueprintReadOnly)
	EProsperitocracyHitMarkerKind Marker = EProsperitocracyHitMarkerKind::Full;

	/**
	 * Whether the marker half of this payload belongs on anyone's screen.
	 *
	 * A blow NOTHING dealt — a dev command hurting you — still owes you its numbers at the body's own
	 * origin, and owes no X: a marker is an answer about damage YOU dealt, and there is no such man here.
	 * The one door enforces it (UProsperitocracyDamageFeedbackStatics), so no sender has to remember.
	 */
	UPROPERTY(BlueprintReadOnly)
	bool bCarriesMarker = true;
};
