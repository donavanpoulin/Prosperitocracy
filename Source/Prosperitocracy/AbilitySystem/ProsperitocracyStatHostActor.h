// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/ProsperitocracyAbilitySourceInterface.h"

#include "ProsperitocracyStatHostActor.generated.h"

class APawn;
class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyStatTable;
class UProsperitocracyThingStatSet;
enum class EProsperitocracyStat : uint8;

/**
 * AProsperitocracyStatHostActor
 *
 * The GAS home for a THING (a weapon, ability, turret, mech, deployable). GAS evaluates
 * attributes on an ability system component, and a thing is an item (a UObject), not an
 * actor — so every thing gets one of these: a lightweight, invisible, non-ticking actor
 * that owns the thing's ASC + UProsperitocracyThingStatSet. The thing's stats are GAS
 * attributes on this ASC, evaluated by the ONE aggregator: (base + Σflat) × Σpercent.
 *
 * Spawned by the thing (the weapon instance, on equip), owned by the pawn; destroyed when
 * the thing is unequipped. Bases are pushed from the thing's stat block by
 * InitializeFromStatBlock. (Co-op replication of this host is a Phase 8 concern.)
 */
UCLASS()
class AProsperitocracyStatHostActor : public AActor, public IProsperitocracyAbilitySourceInterface
{
	GENERATED_BODY()

public:
	AProsperitocracyStatHostActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;

	/** The thing's ASC — the only place its stats are evaluated. */
	UProsperitocracyAbilitySystemComponent* GetProsperitocracyAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** The thing's stat set on its own ASC. */
	UProsperitocracyThingStatSet* GetThingStatSet() const { return ThingStatSet; }

	/**
	 * Push every (stat, base) in the thing's stat block into the set as attribute bases
	 * (the same act as HealthComponent feeding Health). Presence-is-scope: a stat absent
	 * from the block keeps its base 0.
	 */
	void InitializeFromStatBlock(const UProsperitocracyStatTable* StatBlock);

	/**
	 * Write ONE stat's base on this host — the thing's own door onto the one evaluator, whose
	 * current value is (base + Σflat) × Σpercent.
	 *
	 * Two callers: InitializeFromStatBlock (the thing's AUTHORED numbers) and anything whose base is
	 * DERIVED by one fixed formula rather than authored — the bash's Impact Damage is 20 x the gun's
	 * Weight, so its owner pushes that base before the hit resolves. Either way the number above the
	 * base (perks, items) keeps resolving through the aggregator, untouched by who wrote the base.
	 *
	 * Thing stats only: a character stat (Health, Move Speed) belongs to the actor's own ASC and is
	 * never written here.
	 */
	void SetStatBase(EProsperitocracyStat Stat, float BaseValue);

	/**
	 * The ONE push formula, universal — the same three constants for every thing in the game.
	 *
	 * A thing's push is `PushBasePercent + (Weight lbs x PushPercentPerWeightLb) + (damage x
	 * PushPercentPerDamagePoint)` PERCENT of the speed the character is moving at, where damage is the
	 * thing's Impact and Piercing added together (a thing with one line uses that line; a hybrid sums
	 * both). It is written onto the thing's own Drag stat when its block is installed and read from
	 * there by everything else, so there is one number with one home and the aggregator still resolves
	 * perks on top of it.
	 *
	 * Two dials because both are what actually shoves you: a heavy thing and a hard-hitting thing each
	 * push, and neither alone gets to the top — a weight-reduction perk costs push, and damage buys it
	 * back. All three constants are [TUNE].
	 */
	static constexpr float PushBasePercent = 10.0f;
	static constexpr float PushPercentPerWeightLb = 2.0f;
	static constexpr float PushPercentPerDamagePoint = 0.12f;

	/**
	 * Carry is always half of Drag: one push, one formula, read at half when it is helping you instead
	 * of fighting you. Not a second calculation and not a second constant to tune. [TUNE]
	 */
	static constexpr float CarryShareOfDrag = 0.5f;

//~IProsperitocracyAbilitySourceInterface — the host IS an ability source: it outlives the
	// thing that spawned it (an ability instance dies at EndAbility; the host lives until the
	// thing is removed), so delayed damage (a grenade detonating after the ability ended) can
	// safely resolve its lines + falloff from the host's evaluated stats.
	virtual float GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr) const override;

	virtual void GetDamageLines(TArray<FProsperitocracyDamageLine>& OutLines) const override;
	//~End of IProsperitocracyAbilitySourceInterface

protected:
	/**
	 * The thing's OWN numbers that it does not author because they are what its authored numbers ARE:
	 * the push its Weight and damage are worth, as Drag and Carry on this host. Run whenever the
	 * thing's block is installed; both are stat bases, so perks resolve on top of them like any stat.
	 */
	void ApplyDerivedStats();

	/** FINAL value of a stat on this host's own ASC (the ONE evaluator). */
	float GetStatFinal(EProsperitocracyStat Stat) const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Stat")
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Prosperitocracy|Stat")
	TObjectPtr<UProsperitocracyThingStatSet> ThingStatSet;
};
