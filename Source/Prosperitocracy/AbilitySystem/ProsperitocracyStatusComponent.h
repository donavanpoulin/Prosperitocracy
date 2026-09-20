// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "Stats/ProsperitocracyStat.h"
#include "Templates/SubclassOf.h"

#include "ProsperitocracyStatusComponent.generated.h"

class AProsperitocracyStatHostActor;
class UAbilitySystemComponent;
class UGameplayEffect;
class UProsperitocracyStatTable;

/**
 * FProsperitocracyLiveStatus
 *
 * ONE status running on one body: which status it is, the block that IS its numbers, the GAS home
 * those numbers are evaluated on, who set it, and when it runs out.
 *
 * Nothing here is a number of its own. Every value (how long it lasts, how often it ticks, what a
 * tick is worth) is read off the status's own block through the ONE evaluator, so a Duration perk
 * moves a stun's length the same way it would move a burn's, and a status can never be priced at a
 * companion's numbers.
 */
USTRUCT()
struct FProsperitocracyLiveStatus
{
	GENERATED_BODY()

	/** Which status this is — the tag the body carries while it lasts (Status.Stun / Status.Burn). */
	FGameplayTag StatusTag;

	/** The status's OWN block. Its Duration is this status's length, never the applier's. */
	UPROPERTY()
	TObjectPtr<const UProsperitocracyStatTable> Block = nullptr;

	/**
	 * The status's own GAS home — a stat host holding its block, so its numbers are READ through the
	 * one aggregator rather than off an authored base. That is what makes "Duration perks extend the
	 * stun" true later without a line of new code.
	 */
	UPROPERTY()
	TObjectPtr<AProsperitocracyStatHostActor> Host = nullptr;

	/** Who set it. The tick's damage is theirs, and it runs through the same pipeline as their shots. */
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;

	/** The damage effect the one pipeline runs on — the same asset every gun's shot uses. */
	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffectClass = nullptr;

	/** Where it landed: the part that caught it. A status that works on a part reads it; a body-level
	 *  one (burn, which reaches the target itself) does not. */
	FHitResult Hit;

	/** World seconds when it runs out. */
	float EndTime = 0.0f;

	/** World seconds of the next tick — only meaningful for a status that ticks (Burn). */
	float NextTickTime = 0.0f;
};

/**
 * UProsperitocracyStatusComponent
 *
 * The ONE place a status is put on a body, and the ONE place a status behaves — DESIGNED 2026-09-20,
 * to the user's spec (Design/abilities.md, Design/combat.md).
 *
 * What a status IS, in one rule with no status vocabulary and nothing per-status in code:
 *
 *   - A block that carries Rate AND Piercing Damage deals that damage every 1/Rate, for as long as its
 *     Duration lasts — that is Burn.
 *   - A block that carries only Duration simply lasts that long — that is Stun.
 *
 * Its numbers are the status's own (its own block, its own host), so a thing can have a Duration of
 * its own and hand out a differently-long status; the applier never lends its own.
 *
 * Behaviour a status has, beyond its numbers:
 *
 *   - **No stacking.** Reapplied, the dominant instance wins, and dominant is decided at the moment it
 *     lands: the new instance's full value against the current instance's remaining value. For a
 *     ticking status the value is the damage still to come (Rate x Piercing Damage x time left); for a
 *     status that only lasts, it is the time left.
 *   - **A tick's damage is a normal Piercing damage line**, through the same pen-gate → resist path as
 *     a bullet, from the status's own block (its Penetration against the part it caught). It carries no
 *     falloff, because the block carries no Range/Falloff — presence is scope, not a special case.
 *   - **Stun takes the controls away and nothing else.** The body is stopped dead at the instant it
 *     lands, and its own input is gone for as long as the status lasts. Everything else keeps running:
 *     a shove still moves it, gravity still pulls it, so a body stunned in the air drops and lands.
 */
UCLASS(ClassGroup = (Prosperitocracy), meta = (BlueprintSpawnableComponent))
class UProsperitocracyStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProsperitocracyStatusComponent();

	/**
	 * Put ONE status on this body, from the entry that applied it (UProsperitocracyStatTable::
	 * AppliedEffects). The status's own block is the only source of its numbers.
	 *
	 * Server-authoritative: a status is decided once, where the hit was resolved.
	 */
	void ApplyStatus(const FGameplayTag& StatusTag, const UProsperitocracyStatTable* Block,
		UAbilitySystemComponent* SourceAbilitySystemComponent,
		TSubclassOf<UGameplayEffect> DamageEffectClass, const FHitResult& Hit);

	/** The statuses running on this body right now — read by anything that must ask, never to re-decide. */
	const TArray<FProsperitocracyLiveStatus>& GetLiveStatuses() const { return LiveStatuses; }

	/** True while a status that stops movement is on this body. */
	bool IsMovementStopped() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** This body's ability system — where a status's tag is stamped and read. */
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;

	/** The live entry for a status tag, if this body has one. */
	FProsperitocracyLiveStatus* FindLive(const FGameplayTag& StatusTag);

	/** A status's FINAL value read off its own block through the one evaluator. 0 = the block lacks it. */
	float GetStatusStat(const FProsperitocracyLiveStatus& Live, EProsperitocracyStat Stat) const;

	/** Seconds left on a status, never negative. */
	float GetSecondsLeft(const FProsperitocracyLiveStatus& Live, float Now) const;

	/**
	 * What a status is worth for the no-stacking comparison: the damage still to come for a status that
	 * ticks, the seconds left for one that only lasts. One number, either way — the same act decides
	 * both statuses, because which one applies IS presence-is-scope on the block.
	 */
	float GetStatusValue(const FProsperitocracyLiveStatus& Live, float RemainingSeconds) const;

	/** Does this status tick damage? True when its block carries Rate and Piercing Damage. */
	static bool DoesTick(const FProsperitocracyLiveStatus& Live);

	/** How often a ticking status ticks: one over its own Rate. 0 when it carries none. */
	float GetTickInterval(const FProsperitocracyLiveStatus& Live) const;

	/** A status's time is up: it ends, and its GAS home goes with it. */
	void EndStatus(int32 Index);

	/** One tick of a ticking status: one normal Piercing damage line, through the one pipeline. */
	void ApplyTickDamage(const FProsperitocracyLiveStatus& Live);

	/** Take the controls away / give them back, as the movement-stopping status lands and ends. */
	void UpdateMovementGate();

	/** Every status this body is carrying. */
	UPROPERTY()
	TArray<FProsperitocracyLiveStatus> LiveStatuses;

	/** True while the movement gate is closed, so a status landing and ending each act exactly once. */
	bool bMovementStopped = false;
};
