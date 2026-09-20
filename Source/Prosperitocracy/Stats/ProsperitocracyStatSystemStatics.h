// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyStatSystemStatics.generated.h"

class UAbilitySystemComponent;
class UProsperitocracyStatTable;
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
	 * Whether this stat's GAS home is one of the BODY's own sets — a character stat (Health, Move
	 * Speed, the two resists) rather than a thing's (a gun's damage, an armor's weight).
	 *
	 * It is the one answer to "whose number is this", and it is asked in BOTH directions when a block
	 * is pushed: a thing stat in a block handed to a body is not a body number, and a body stat in a
	 * thing's block is not a thing number. Presence-is-scope, asked once, from one place.
	 */
	static bool IsCharacterStat(EProsperitocracyStat Stat);

	/**
	 * The body's own resist row that answers a line of a given damage type — Impact Resist for an Impact
	 * line, Piercing Resist for a Piercing line.
	 *
	 * The ONE place that pairing is written down, so no receiver keeps its own copy of it: an enemy part
	 * asking which of its block's rows is the one it wears, and a player's body asking which of its own
	 * rows the weave filled, both come here. Absent from the vocabulary are exactly two damage types
	 * (Design/damage.md), so anything that is not Impact is Piercing.
	 */
	static EProsperitocracyStat GetResistStatForDamageType(FGameplayTag DamageType);

	/**
	 * Write a BLOCK's BODY rows onto an ASC — the receiver-side numbers, which today are the two resists —
	 * or take them back off with bBare. A row that belongs to a THING's own home is skipped: it is not the
	 * body's number, and pushing it here would put a gun's or an armor's Weight on a character.
	 *
	 * The ONE door for a block's body rows. A body dressed by a weave and a body part dressed by its own
	 * block go through this same call, so neither can end up dressed differently — and a block carrying
	 * rows for both homes (a part block: its armour AND its resist) is dressed by asking both homes, one
	 * call each, never by handing the whole block to one of them and hoping.
	 */
	static void ApplyBlockBodyRows(UAbilitySystemComponent* ASC, const UProsperitocracyStatTable* Block, bool bBare);

	/**
	 * The ONE universal falloff (Design/damage.md): full damage from 0 to Falloff, then a LINEAR
	 * ramp down to 0 at Range. Distance is UE cm (traces), Range/Falloff are the stats in meters.
	 * Absent stats (<= 0) = no falloff (presence-is-scope). Every source resolves its damage
	 * distance through this — guns and explosions, one formula.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stat")
	static float ComputeDistanceAttenuation(float Distance, float RangeMeters, float FalloffMeters);
};
