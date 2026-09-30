// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ProsperitocracyAbilitySourceInterface.generated.h"

class UObject;
class UPhysicalMaterial;
class UProsperitocracyStatTable;
struct FGameplayTagContainer;
struct FProsperitocracyDamageLine;
enum class EProsperitocracyStat : uint8;

/** Base interface for anything acting as a ability calculation source */
UINTERFACE()
class UProsperitocracyAbilitySourceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IProsperitocracyAbilitySourceInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 * Compute the multiplier for effect falloff with distance
	 * 
	 * @param Distance			Distance from source to target for ability calculations (distance bullet traveled for a gun, etc...)
	 * @param SourceTags		Aggregated Tags from the source
	 * @param TargetTags		Aggregated Tags currently on the target
	 * 
	 * @return Multiplier to apply to the base attribute value due to distance
	 */
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const = 0;

	/**
	 * The damage line(s) this source deals (data-driven: the weapon/ability knows its own damage).
	 * The shared execution reads the context's lines first; if there are none it falls back to here.
	 */
	virtual void GetDamageLines(TArray<FProsperitocracyDamageLine>& OutLines) const
	{
	}

	/**
	 * The thing's STAT BLOCK — what it IS: the rows it carries, the statuses it applies, and any mark
	 * it wears (Design/explosions.md's explosion tag).
	 *
	 * It lives on this interface because a source that cannot say what it IS forces its caller to
	 * bring its own copy of the answer — and two copies of "which thing is this" is exactly how a mark
	 * ends up on one block while the numbers resolve from another. One question, asked of the thing.
	 */
	virtual const UProsperitocracyStatTable* GetStatBlock() const = 0;

	/**
	 * FINAL value of one of the thing's stats, through its own GAS home's aggregator —
	 * (base + Σflat) × Σpercent, the ONE evaluator. An absent stat is 0 (presence-is-scope).
	 *
	 * A source answers for its own numbers rather than handing a caller a block to read a raw base off:
	 * a raw-base read is a build-blocking violation everywhere in this project, and a consumer of a
	 * thing — the explosion pass measuring a ball off the thing's Range — needs the evaluated number
	 * like every other reader does.
	 */
	virtual float GetStatFinalValue(EProsperitocracyStat Stat) const = 0;
};
