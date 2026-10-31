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
 * ONE status running on one body: which block it came from, the GAS home that block's numbers are
 * evaluated on, who set it, when it lands, when it runs out and when it next ticks.
 *
 * The block IS the identity — two things that set the same block are the same version, however many of
 * them hit — and the block is also the only source of every number: how long it lasts, how often it
 * ticks, what a tick is worth. Nothing here is a number of its own, so a Duration perk moves a stun's
 * length the same way it would move a burn's.
 *
 * Its LOOK is not here, and never was one-per-status in the way that matters: the fire a burning body
 * wears belongs to the body, and lives on the queue below.
 */
USTRUCT()
struct FProsperitocracyLiveStatus
{
	GENERATED_BODY()

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
 * FProsperitocracyStatusQueue
 *
 * ONE KIND of status on one body, oldest first. Only Waiting[0] — the head — applies: it ticks, it is
 * the one stunning you, it is what the body looks like. The rest are behind it, and their clocks have
 * been running since the moment each of them landed, because a status's clock is its own and no
 * application starts it.
 *
 * What that buys, in his words: you feel the full burn, you cannot cheese it by taking a less bad burn,
 * and you cannot cheese it by leaving the fire when the first burn ends.
 *
 * The LOOK and the LIGHT live here rather than on one entry, because they belong to the body: a body
 * that is burning is on fire once, and a handover changes the damage and the rate, never the fire.
 */
USTRUCT()
struct FProsperitocracyStatusQueue
{
	GENERATED_BODY()

	/** Which status this queue is — the tag the body carries while anything in it lasts. */
	FGameplayTag StatusTag;

	/** Oldest first. Waiting[0] is the head, and only the head applies. */
	UPROPERTY()
	TArray<FProsperitocracyLiveStatus> Waiting;

	/**
	 * The effect this body wears for as long as THIS queue has anything in it — on the chest, never on
	 * the capsule, and told to stop when the last one leaves. A status absent from the look pairing has
	 * none, and is left alone.
	 */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Effect = nullptr;

	/** The light this body throws while this queue is live. OURS, one per body, never one per particle. */
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
 *   - **A queue per kind, and only the head applies** (his rule, 2026-10-30). Every kind of status has
 *     its own queue, oldest first; the two queues are parallel, so a body can be burning and stunned at
 *     once. Each status's clock runs from the moment IT landed, whether it is applying or not: the one
 *     behind is already burning down while it waits, and it takes over with what it has left when the
 *     head expires — or is dropped, if its own time runs out first.
 *     The same block landing again moves that status's end time to a full clock and NOT its next tick:
 *     a fire re-applies every fifth of a second, and a refresh that pushed the tick back once left a
 *     body burning with no damage at all until it walked out.
 *   - **A tick's damage is a normal Piercing damage line**, through the same pen-gate → resist path as
 *     a bullet — but it carries NO pen: a status is not an attack that penetrates anything, so nothing
 *     can bounce it off a plate and what answers it is the target's own Piercing Resist. It carries no
 *     falloff either, because the block carries no Range/Falloff — presence is scope, not a special case.
 *   - **A stun closes one gate and takes part of his speed** (PLANS/effects.md). It is not the controller,
 *     not the input stack and not a tag: the head stun says when he may act again, every door the player
 *     has asks that one question, and the speed he keeps is ONE number for every stun in the game.
 *     Gravity and a shove are untouched: a body stunned in the air drops and lands.
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
	 * A block this body is already carrying refreshes its own place; a different block joins the back.
	 *
	 * Server-authoritative: a status is decided once, where the hit was resolved.
	 */
	void ApplyStatus(const FGameplayTag& StatusTag, const UProsperitocracyStatTable* Block,
		UAbilitySystemComponent* SourceAbilitySystemComponent,
		TSubclassOf<UGameplayEffect> DamageEffectClass, const FHitResult& Hit);

	/**
	 * Whether this body may act at all — THE ONE GATE, and the only answer to it in the game.
	 *
	 * A stun is the status that forbids acting: every door the player has asks this, the ability system's
	 * own input door asks this, and the jump asks this. There is no tag, no controller and no input stack
	 * involved, and nothing else records "stunned" anywhere.
	 */
	bool AllowsActing() const;

	/**
	 * The share of his own Move Speed this body may use right now — 1 when nothing takes any.
	 *
	 * It is the head stun as the wire carries it: when it ends, and the share he keeps — resolved on the
	 * server off the block's own row, through the one evaluator. Its last half second hands the share
	 * back, reaching 1 on the tick the status ends, so the legs coming back and the controls coming back
	 * are one event.
	 */
	float GetSpeedShare() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** This body's ability system — where a status's tag is stamped and read. */
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;

	/** The queue for a status tag, or null when this body carries none of it. */
	FProsperitocracyStatusQueue* FindQueue(const FGameplayTag& StatusTag);
	const FProsperitocracyStatusQueue* FindQueue(const FGameplayTag& StatusTag) const;

	/** The entry in this queue that came from that block — the status's identity — or null. */
	static FProsperitocracyLiveStatus* FindWaiting(FProsperitocracyStatusQueue& Queue, const UProsperitocracyStatTable* Block);

	/** A status's FINAL value read off its own block through the one evaluator. 0 = the block lacks it. */
	float GetStatusStat(const FProsperitocracyLiveStatus& Live, EProsperitocracyStat Stat) const;

	/** Does this status tick damage? True when its block carries Rate and Piercing Damage. */
	static bool DoesTick(const FProsperitocracyLiveStatus& Live);

	/** How often a ticking status ticks: one over its own Rate. 0 when it carries none. */
	float GetTickInterval(const FProsperitocracyLiveStatus& Live) const;

	/**
	 * The head's time is up: it ends, its GAS home goes with it, and the one behind it becomes the head
	 * with the time it has left. When nothing is left, the queue's look goes out and the tag comes off.
	 */
	void EndTheHead(int32 QueueIndex);

	/**
	 * The tick phase of a status that has just BECOME the front — landing on an empty queue, or being
	 * promoted when the one ahead ends. Nothing strikes while a status waits, so nothing is owed when it
	 * takes over: its intervals count from the turn it got.
	 */
	void BeginItsTicks(FProsperitocracyLiveStatus& Head, float Now);

	/** Drop the ones whose own clock ran out while they were still waiting: they never apply at all. */
	void DropExpiredWaiters(FProsperitocracyStatusQueue& Queue, float Now);

	/** One tick of a ticking status: one normal Piercing damage line, through the one pipeline. */
	void ApplyTickDamage(const FProsperitocracyLiveStatus& Live);

	/**
	 * Put this queue's own look on the body, on its chest. Called when the queue goes from empty to one —
	 * never again while it lasts, because the fire belongs to the body and a handover changes numbers,
	 * not the fire.
	 */
	void BeginStatusEffect(FProsperitocracyStatusQueue& Queue);

	/**
	 * Tell this queue's look to stop: no more fire is spawned and the flames already on the body burn
	 * out at their own pace, then the effect destroys itself. A stop, never a cut. The light is handed to the
	 * ones going out, so it leaves over the same second the flames do instead of cutting with the status.
	 */
	void EndStatusEffect(FProsperitocracyStatusQueue& Queue);

	/**
	 * THE LIGHT THIS BURNING BODY THROWS — made once, when the burn lands, hung on the same chest the fire hangs
	 * on. Ours, not the effect's: one light that rides the body instead of one light per particle strobing on it.
	 */
	void BeginTheLightOnThisBody(FProsperitocracyStatusQueue& Queue, USceneComponent* AttachTo, FName OnBone, float Now);

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

	/** The share of his own speed a stunned body keeps — ONE number for every stun in the game. A stun
	 *  does not get better or worse, only longer or shorter: its Duration is the only thing a source
	 *  authors, and that lives on the source's own block. HIS 15%. */
	static constexpr float HowMuchSpeedAStunLeavesHim = 0.15f;

	/** How long a stun takes to give his speed back at the end — HIS 0.5 s, and its own number: the
	 *  shot's push window is a different feature's half second and tuning one must not move the other. */
	static constexpr float HowLongAStunGivesTheSpeedBackSeconds = 0.5f;

	/**
	 * THE HEAD STUN, AS THE WIRE CARRIES IT, written from the queue on the server and read by both sides.
	 *
	 * A client cannot own a clock, so the server resolves the two facts a client needs — when the stun
	 * ends, and the share of his speed he keeps — off the head's own block. That is the one truth
	 * serialised, not a second copy of a mechanism: every read goes through these two numbers on both
	 * machines, so the server and a client can never disagree about whether he may act.
	 */
	void PublishTheStun();

	/** Every status this body is carrying, one queue per kind, oldest first. */
	UPROPERTY()
	TArray<FProsperitocracyStatusQueue> Queues;

	/** The lights of burns that have ENDED and are still going out: they outlive the status by that second. */
	UPROPERTY()
	TArray<FProsperitocracyBurningBodyLight> LightsGoingOut;

	/** World seconds the head stun ends — past, or in the future, this is when he may act again. */
	UPROPERTY(Replicated)
	float StunEndsAt = 0.0f;

	/** The share of his own Move Speed the head stun leaves him while it lasts. 1 when nothing is on him. */
	UPROPERTY(Replicated)
	float StunSpeedKeep = 1.0f;
};
