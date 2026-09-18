// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilitySystem/ProsperitocracyAbilitySet.h"
#include "Stats/ProsperitocracyStat.h"

#include "ProsperitocracyPlayerStatsComponent.generated.h"

class UCharacterMovementComponent;
class UProsperitocracyAbilitySystemComponent;
class UProsperitocracyHealthSet;
class UProsperitocracyStatSet;
class UProsperitocracyStatTable;

/**
 * UProsperitocracyPlayerStatsComponent
 *
 * The player's own numbers, and the one place they reach the body.
 *
 * Every one of these numbers is a GAS attribute (Design/stats.md: "GAS is the evaluator" — nothing
 * reads a raw base, there is no non-GAS numeric path). This component does three things and nothing
 * else: it puts the baseline stat block into the character's ability system when the character comes
 * up, it grants the abilities the character owns (the bash), and it pushes the movement stats onto
 * the character movement component. Anything that wants a stat asks GAS for its FINAL value, through
 * the same door as everything else (`UProsperitocracyStatSystemStatics::GetStatFinal`).
 *
 * The template's movement is untouched: its walk and run states still decide WHEN each speed
 * applies, and its graph asks this component for the number instead of holding one of its own. That
 * is deliberate — the number has one owner, and a stat that is not set never zeroes the body out
 * (movement keeps whatever it had).
 *
 * There is ONE speed stat (Move Speed = the run speed). Walking is not a second stat: it is that
 * same number times one universal constant, so a walk is always half a run and tuning one number
 * tunes both. Jump is one stat too, and it is a VELOCITY (Jump Velocity, cm/s) because that is what
 * the body is driven by — a height would be the same jump described in a unit the movement cannot use.
 *
 * What the character CARRIES then moves both: every pound of gear costs the same slice of run speed
 * and jump velocity, read from the loadout's carried total. Walking is never scaled directly — it is
 * half of the already-scaled run speed, so it follows. That ordering is the whole design of it: one
 * stat (weight) feeds one formula (percent per pound) which scales two numbers (run, jump), and the
 * third (walk) is derived from one of them.
 */
UCLASS(ClassGroup = (Prosperitocracy), meta = (BlueprintSpawnableComponent))
class UProsperitocracyPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProsperitocracyPlayerStatsComponent();

	/** What this character's numbers ARE: one entry per stat it has. Presence is scope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Stats")
	TObjectPtr<UProsperitocracyStatTable> BaselineStats;

	/**
	 * What this character OWNS as abilities — the gun bash today.
	 *
	 * Granted to the character's ability system when it comes up, through the project's own ability
	 * set (the same data asset the ability's input tag travels in). One home for a granted ability:
	 * nothing else has to remember what the character has, and nothing holds a spec handle of its own.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Prosperitocracy|Abilities")
	TObjectPtr<UProsperitocracyAbilitySet> AbilitySet;

	/** FINAL value of one of this character's stats, through GAS — the ONE evaluator. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetStat(EProsperitocracyStat Stat) const;

	/** The run speed: Move Speed, through the weight penalty. Running is the number; walk is a fraction. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetRunSpeed() const { return GetStat(EProsperitocracyStat::MoveSpeed) * GetWeightSpeedMultiplier(); }

	/** Walking: the same Move Speed through the one walk multiplier, weight penalty included with it. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetWalkSpeed() const;

	/** The jump's launch velocity: Jump Velocity through the weight penalty, onto the movement component. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetJumpVelocity() const { return GetStat(EProsperitocracyStat::JumpVelocity) * GetWeightSpeedMultiplier(); }

	/** Everything this character carries, in lbs, from its loadout. 0 when it carries nothing. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetCarriedWeightLbs() const;

	/**
	 * The weight penalty: what the carried weight does to run speed and jump velocity.
	 *
	 * It is a multiplier, not a delta, because it has to compose with everything else that already
	 * moved those numbers (a perk, an attachment) without knowing any of them: the stat's FINAL value
	 * is what gets scaled. Walking is not scaled again — it is half of the already-scaled run speed.
	 *
	 * This is the one formula, with one constant, for every character and every item: heavier is
	 * slower, and there is nowhere else that decides how much.
	 */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetWeightSpeedMultiplier() const;

	/**
	 * Push the movement numbers onto the character movement component: walk and run speed from Move
	 * Speed, jump velocity from Jump Velocity. Safe to call again after the numbers change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void ApplyToMovement();

	/**
	 * The jump number on its own. Safe at any moment, because no movement state writes JumpZVelocity —
	 * so this is what a change in what the character carries re-applies, while the walk and run speeds
	 * wait for the movement state that owns them to read the stat again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void ApplyJumpVelocity();

	/**
	 * The one walk multiplier, universal — every character walks at the same fraction of its run
	 * speed, so a slower character is slower walking too. It is not a stat and not a perk target:
	 * the stat is the run speed, and this is the fraction that turns it into a walk.
	 */
	static constexpr float WalkSpeedMultiplier = 0.5f;

	/**
	 * The one weight constant: percent of run speed and jump velocity that one pound costs.
	 *
	 * One number for every weapon, every armor, every character — Design/loadout.md: "Total weight
	 * reduces everything: jump height, walk/run speed, etc.", and Design/stats.md: a derived value is
	 * computed by ONE fixed formula with universal constants. This is that constant. [TUNE]
	 */
	static constexpr float WeightPenaltyPercentPerLb = 0.5f;

protected:
	virtual void BeginPlay() override;

private:
	/** Push the baseline block into the character's attributes, once per life. */
	void ApplyBaselineStats();

	/**
	 * Grant what this character owns as abilities (the bash), once per life.
	 *
	 * Through the project's own ability set, so an ability is granted in one place, carries its input
	 * tag with it, and can be handed back as a set if the character ever stops owning it.
	 */
	void GrantAbilities();

	/** What was granted, so it could be taken away again. */
	FProsperitocracyAbilitySet_GrantedHandles GrantedAbilityHandles;

	/** The owning pawn's ability system — where every stat of theirs lives. */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	/** One warning per component, not one per read, when there is no baseline to apply. */
	bool bBaselineWarned = false;
};
