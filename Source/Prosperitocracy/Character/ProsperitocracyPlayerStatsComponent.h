// Copyright Prosperitocracy. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
 * reads a raw base, there is no non-GAS numeric path). This component does two things and nothing
 * else: it puts the baseline stat block into the character's ability system when the character comes
 * up, and it pushes the movement stats onto the character movement component. Anything that wants a
 * stat asks GAS for its FINAL value, through the same door as everything else
 * (`UProsperitocracyStatSystemStatics::GetStatFinal`).
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

	/** FINAL value of one of this character's stats, through GAS — the ONE evaluator. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetStat(EProsperitocracyStat Stat) const;

	/** The run speed: Move Speed as it stands. Running is the number; walking is a fraction of it. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetRunSpeed() const { return GetStat(EProsperitocracyStat::MoveSpeed); }

	/** Walking: the same Move Speed through the one walk multiplier. The template's walk state reads this. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetWalkSpeed() const;

	/** The jump's launch velocity: Jump Velocity as it stands, straight onto the movement component. */
	UFUNCTION(BlueprintPure, Category = "Prosperitocracy|Stats")
	float GetJumpVelocity() const { return GetStat(EProsperitocracyStat::JumpVelocity); }

	/**
	 * Push the movement numbers onto the character movement component: walk and run speed from Move
	 * Speed, jump velocity from Jump Velocity. Safe to call again after the numbers change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prosperitocracy|Stats")
	void ApplyToMovement();

	/**
	 * The one walk multiplier, universal — every character walks at the same fraction of its run
	 * speed, so a slower character is slower walking too. It is not a stat and not a perk target:
	 * the stat is the run speed, and this is the fraction that turns it into a walk.
	 */
	static constexpr float WalkSpeedMultiplier = 0.5f;

protected:
	virtual void BeginPlay() override;

private:
	/** Push the baseline block into the character's attributes, once per life. */
	void ApplyBaselineStats();

	/** The owning pawn's ability system — where every stat of theirs lives. */
	UPROPERTY(Transient)
	TObjectPtr<UProsperitocracyAbilitySystemComponent> AbilitySystemComponent;

	/** One warning per component, not one per read, when there is no baseline to apply. */
	bool bBaselineWarned = false;
};
