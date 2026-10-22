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
class UNiagaraComponent;
class UPointLightComponent;
class UProsperitocracyStatTable;

/**
 * THE LIGHT OF A BURNING BODY — ours, ONE per body, and it comes up and goes out over its own ramps.
 *
 * It exists because the effect cannot light itself: a light inside Niagara is one light PER PARTICLE, spawning
 * and dying with the cards, which is a light that strobes and snaps instead of a body that is on fire.
 *
 * WhenItWasLit is the frame it came up. WhenItStartedGoingOut is the frame the burn ENDED — from there it goes out
 * over the second the last flames on the body take, so the body's light and the body's fire leave together.
 */
USTRUCT()
struct FProsperitocracyBurningBodyLight
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPointLightComponent> TheLight = nullptr;

	/** What this body is lit with while it burns, in candelas. */
	UPROPERTY()
	float WhatItIsWorth = 0.0f;

	UPROPERTY()
	float WhenItWasLit = 0.0f;

	/** Less than zero while the body is still burning; the moment the burn ended and it began going out. */
	UPROPERTY()
	float WhenItStartedGoingOut = -1.0f;
};

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

	/**
	 * The status's own LOOK — the effect that stands on the body for as long as this status lasts.
	 *
	 * It is hung on the body's CHEST, not on its origin: a body that falls over lies down while its capsule stays
	 * standing, and fire left standing on the capsule is fire in mid-air over a man on the ground. The effect takes
	 * no scale, and keeps its own upright rotation so the flames rise whichever way the body falls.
	 *
	 * Null for a status that wears no effect (the pairing is one place, and a status absent from it has
	 * no look), and null again the moment the status ends — the effect is told to stop and destroys
	 * itself once its last particle dies.
	 */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Effect = nullptr;

	/** The light this body's status throws. OURS, one per body, never a light per particle in the effect. */
	UPROPERTY()
	FProsperitocracyBurningBodyLight Light;
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
 *   - **No stacking, and no taking over either** (his rule, 2026-10-01). A body keeps the status it is
 *     carrying until that one EXPIRES; while one is on, the same status landing again does nothing at
 *     all - no refresh, no reset, no comparison - and the next application starts a fresh one the
 *     moment the last runs out. Universal: every status, every body, every source.
 *     What this replaced: the newcomer's whole value against what was LEFT of the one on the body,
 *     with a winner that reset the clock - and a fire re-applying every fifth of a second reset it
 *     before the burn's first tick could ever land, so a body burned and took no damage at all.
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

	/** Does this status tick damage? True when its block carries Rate and Piercing Damage. */
	static bool DoesTick(const FProsperitocracyLiveStatus& Live);

	/** How often a ticking status ticks: one over its own Rate. 0 when it carries none. */
	float GetTickInterval(const FProsperitocracyLiveStatus& Live) const;

	/** A status's time is up: it ends, and its GAS home goes with it. */
	void EndStatus(int32 Index);

	/** One tick of a ticking status: one normal Piercing damage line, through the one pipeline. */
	void ApplyTickDamage(const FProsperitocracyLiveStatus& Live);

	/**
	 * Put this status's own look on the body, on its chest. Called once, when the status LANDS — never on a
	 * re-application, because a status that is already burning does not light a second fire.
	 */
	void BeginStatusEffect(FProsperitocracyLiveStatus& Live);

	/**
	 * Tell this status's look to stop: no more fire is spawned and the flames already on the body burn
	 * out at their own pace, then the effect destroys itself. A stop, never a cut. The light is handed to the
	 * ones going out, so it leaves over the same second the flames do instead of cutting with the status.
	 */
	void EndStatusEffect(FProsperitocracyLiveStatus& Live);

	/**
	 * THE LIGHT THIS BURNING BODY THROWS — made once, when the burn lands, hung on the same chest the fire hangs
	 * on. Ours, not the effect's: one light that rides the body instead of one light per particle strobing on it.
	 */
	void BeginTheLightOnThisBody(FProsperitocracyLiveStatus& Live, USceneComponent* AttachTo, FName OnBone, float Now);

	/** Its brightness this frame: up over its own ramp, and out over the second after the burn ends. */
	void RetuneTheLightsOfBurningBodies(float Now);

	/** Where the fire hangs on a body: the chest, or the body itself when it has no bones to name. */
	USceneComponent* WhereTheFireHangsOnThisBody(FName& OutBoneName) const;

	/** [TUNE] MINE — the colour a burning body is lit with: the one fire colour in the game, and the only light
	 *  that is fire now that a fire's own ground is lit by nothing but its flames. */
	static const FLinearColor TheColourAFireIsLitWith;

	/** [TUNE] MINE — what a burning body throws, how far it reaches, and how soft its shadow is. A body is small: this
	 *  is a suit of fire lighting the ground around it, not a bonfire. */
	static constexpr float CandelasABurningBodyIsLitWith = 350.0f;
	static constexpr float HowFarTheLightOfABurningBodyReachesCm = 350.0f;
	static constexpr float HowSoftTheBurningBodysLightIsCm = 20.0f;

	/** How long a burning body's light takes to come up — and to go out once the burn is over, which is the
	 *  second the last flames on the body take to die. */
	static constexpr float HowLongABurningBodysLightTakesToComeUpSeconds = 0.2f;
	static constexpr float HowLongTheLightOnABurningBodyTakesToGoOutSeconds = 1.0f;

	/** Take the controls away / give them back, as the movement-stopping status lands and ends. */
	void UpdateMovementGate();

	/** Every status this body is carrying. */
	UPROPERTY()
	TArray<FProsperitocracyLiveStatus> LiveStatuses;

	/** The lights of burns that have ENDED and are still going out: they outlive the status by that second. */
	UPROPERTY()
	TArray<FProsperitocracyBurningBodyLight> LightsGoingOut;

	/** True while the movement gate is closed, so a status landing and ending each act exactly once. */
	bool bMovementStopped = false;
};
