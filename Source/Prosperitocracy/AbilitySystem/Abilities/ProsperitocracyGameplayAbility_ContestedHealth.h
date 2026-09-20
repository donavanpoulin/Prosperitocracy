// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/ProsperitocracyGameplayAbility.h"

#include "ProsperitocracyGameplayAbility_ContestedHealth.generated.h"

class UProsperitocracyHealthSet;

/**
 * UProsperitocracyGameplayAbility_ContestedHealth
 *
 * **Contested health** (Design/combat.md, Design/stats.md; the user's rework, 2026-09-20): the part of a
 * hit you just took that you can still WIN BACK by landing damage.
 *
 *   - A hit takes health off you for good and can kill you — contested health is NOT a shield and never
 *     saves you from a lethal blow.
 *   - What that hit took becomes **contested**, and it does not start leaving straight away: **for one
 *     Duration it sits there, untouched**, and only then **drains evenly across the next Duration** — the
 *     SAME number twice, never two durations (his rule: a pause and a fade would have been two values to
 *     tune, so the one Duration is both).
 *   - **A fresh hit starts the sit over**: during the sit the pause restarts at full, and during the
 *     drain it behaves like a fresh hit — it adds onto what is left and goes back to the sit.
 *   - **Damage you DEAL wins health back**: a flat share of what you dealt, capped at what is still
 *     contested — so the pool is the ceiling and you can never be healed above the health you have.
 *   - What is left when the drain finishes is gone for good.
 *
 * **No rate stat, ever.** The drain's speed is derived — pool ÷ Duration, worked out when the drain
 * begins — deliberately: a knob that raised your OWN decay could only ever be built to hurt you, so it
 * does not exist. The ONE number is the **Duration** row of this ability's own block, and Duration perks
 * lengthen both halves of the window, which is pure upside.
 *
 * It is a passive every body has — no slot, nothing ever pressed — so it activates itself when the
 * avatar is assigned and lives as long as the body does.
 */
UCLASS()
class UProsperitocracyGameplayAbility_ContestedHealth : public UProsperitocracyGameplayAbility
{
	GENERATED_BODY()

public:
	UProsperitocracyGameplayAbility_ContestedHealth();

	/**
	 * How much of the damage you DEAL wins health back: an eighth.
	 *
	 * A universal constant, not a stat and not a perk target — never bought, never tuned per thing, and
	 * never a knob that could be turned against the player. It came down from a quarter at his call: a
	 * quarter won health back too easily, so the recovery is a smaller fixed slice. [TUNE] as a number he
	 * owns, but it is not a table row and never becomes one.
	 */
	static constexpr float ShareOfDamageDealtRecovered = 0.125f;

	/**
	 * THE ONE DOOR for the pool, called by the damage pipeline: this body just TOOK this much damage, so
	 * that much becomes contested — added onto whatever is left, with the window restarted and the rate
	 * re-worked out from the new pool.
	 */
	static void NotifyDamageTaken(AActor* Body, float DamageTaken);

	/**
	 * THE ONE DOOR for the recovery, called by the damage pipeline: this body just DEALT this much, so a
	 * flat quarter of it wins health back, capped at what is still contested.
	 */
	static void NotifyDamageDealt(AActor* DamageDealer, float DamageDealt);

	/** What a body is holding as contested right now, or 0 when it has none (the white part of its bar). */
	static float GetContestedHealth(AActor* Body);

	/** The body's contested pool. */
	float GetContested() const { return Contested; }

	/**
	 * The window the pool fades across — this ability's own Duration row, read FINAL, so a Duration perk
	 * moves it. 0 when the ability has no block, which means no window and nothing ever contested.
	 */
	float GetWindowSeconds() const;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** A hit landed on this body: that much joins the pool, and the pool goes back to sitting still. */
	void TakeContested(float DamageTaken);

	/** Damage came off this body's hands: a flat share of it wins health back. */
	void RecoverFromDamageDealt(float DamageDealt);

	/** The clock: the sit counts down, then the drain takes the pool away — same Duration, twice. */
	void FadeStep();

	/**
	 * The pool sits still for one whole Duration from here.
	 *
	 * This is the whole trick of his rework: a pause and a fade would have been TWO durations to author
	 * and to tune, so the one Duration is used twice — sit for it, then drain over it.
	 */
	void StartSit();

	/** This body's health set, or null when it has none. */
	UProsperitocracyHealthSet* GetHealthSet() const;

	/** Where in the window this body is: nothing on the clock, sitting still, or draining away. */
	enum class EPhase : uint8
	{
		Idle,
		Sitting,
		Draining
	};

	/** What is held right now, in health. */
	float Contested = 0.0f;

	/** The pool over the Duration, worked out when the drain begins. Never a stat, never a row. */
	float DrainPerSecond = 0.0f;

	/** How much of the current phase is left — the sit, or the drain. */
	float PhaseSecondsRemaining = 0.0f;

	EPhase Phase = EPhase::Idle;

	FTimerHandle FadeTimerHandle;
};
