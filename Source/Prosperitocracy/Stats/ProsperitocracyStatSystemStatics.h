// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyStatSystemStatics.generated.h"

class UAbilitySystemComponent;
struct FGameplayAttribute;

/**
 * UProsperitocracyStatSystemStatics
 *
 * THE single door into the universal stat table (Design/stats.md) and its one value
 * evaluator. Every stat in the game:
 *   - is addressed by an EProsperitocracyStat (vocabulary),
 *   - lives on a GAS attribute (see GetAttributeForStat),
 *   - is EVALUATED by the GAS attribute aggregator — (base + Σflat) × Σpercent —
 *     which this class never re-implements (that would be a second system).
 *
 * Modifying a stat is always the same act: author a Gameplay Effect whose modifier
 * targets the stat's attribute (flat => Additive, percent => Multiplicitive). That GE
 * routes through the one aggregator, so a perk delta and an ability base speak the same
 * language. (Perks/attachments applying those GEs is a later phase; this class only
 * exposes the vocab → attribute mapping and the base→final read for the UI.)
 *
 * Presence-is-scope: a stat only resolves on an ASC that actually owns the mapped
 * attribute. If GetAttributeForStat returns an invalid attribute, the stat is not
 * present on that actor.
 */
UCLASS(BlueprintType)
class UProsperitocracyStatSystemStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the GAS attribute that backs stat, or an invalid attribute if no ASC-backed home exists yet (presence-is-scope). */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stat")
	static FGameplayAttribute GetAttributeForStat(EProsperitocracyStat Stat);

	/** The BASE value of a stat on an ASC (pre-modifiers). For the UI "base → final" read. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stat")
	static float GetStatBase(const UAbilitySystemComponent* ASC, EProsperitocracyStat Stat);

	/** The FINAL (aggregator-evaluated) value of a stat on an ASC = (base + Σflat) × Σpercent. For the UI "base → final" read. */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stat")
	static float GetStatFinal(const UAbilitySystemComponent* ASC, EProsperitocracyStat Stat);

	/**
	 * The ONE universal falloff (Design/damage.md): full damage from 0 to Falloff, then a LINEAR
	 * ramp down to 0 at Range. Distance is UE cm (traces), Range/Falloff are the stats in meters.
	 * Absent stats (<= 0) = no falloff (presence-is-scope). Every source resolves its damage
	 * distance through this — guns and explosions, one formula.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stat")
	static float ComputeDistanceAttenuation(float Distance, float RangeMeters, float FalloffMeters);
};
